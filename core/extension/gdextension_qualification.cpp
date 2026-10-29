#include "gdextension_qualification_internal.h"
#include "gdextension.h"
#include "core/crypto/crypto_core.h"
#include "core/os/os.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/rendering_shader_container.h"
#include "core/os/thread.h"
#include "core/object/object.h"

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error error, uint32_t reason=0) { return {int32_t(error),reason,0,0}; }
template<class T> bool aligned(const T *p) { return p && uintptr_t(p)%alignof(T)==0; }
bool separate(const void *a,size_t as,const void *b,size_t bs) {
 auto x=uintptr_t(a),y=uintptr_t(b); return x<=y ? as<=y-x : bs<=x-y;
}
bool valid_abi(const GDExtensionDeviceExitAbi *abi) { return aligned(abi) && abi->abi_version==1 && abi->struct_size==sizeof(*abi) && !abi->reserved; }
}
Error Registry::qualification_bridge_locked(GDExtensionQualificationBridgeRefV1 ref,bool execution,bool admit,QualificationBridge *&out,RenderingDevice *&rd,uint32_t accepted_kind) {
 if(ref.registry_instance!=instance || !ref.handle_serial)return ERR_INVALID_DATA;
 for(auto *bridge:qualification_bridges) {
  if(!bridge || bridge->serial!=ref.handle_serial)continue;
  auto &record=records[bridge->library]; auto &device=device_records[bridge->device];
  if(!device.live || device.epoch!=bridge->epoch || !device.object)return ERR_UNAVAILABLE;
  if(record.owner_thread!=Thread::get_caller_id() && !execution)return ERR_UNAUTHORIZED;
  rd=Object::cast_to<RenderingDevice>(device.object);
  if(!rd || !rd->is_main_instance || rd!=RenderingDevice::get_singleton())return ERR_INVALID_DATA;
  if(execution && (device.execution_thread!=Thread::get_caller_id() || rd->render_thread_id!=Thread::get_caller_id()))return ERR_UNAUTHORIZED;
  const bool accepted=admit && execution && qualification_accepted_call_locked(bridge,accepted_kind);
  if(admit && (record.guard_enrollment!=GuardEnrollment::QUALIFIED || ((bridge->closing || record.quiescing || record.close_started) && !accepted)))return ERR_BUSY;
  if(!bridge->holder || bridge->holder->pending || bridge->holder->generation!=record.snapshot.library_generation)return ERR_INVALID_DATA;
  out=bridge;return OK;
 }
 return ERR_DOES_NOT_EXIST;
}
GDExtensionDeviceSessionResultV1 Registry::qualification_open(const GDExtensionDeviceExitAbi *abi,GDExtensionDeviceExitSceneReadyContextRef frame,GDExtensionDeviceRegistryContextRef context,GDExtensionOriginalDeviceSessionRef session,GDExtensionDeviceExitLibraryCodeHoldRef code,GDExtensionQualificationBridgeRefV1 *out) {
 if(!valid_abi(abi) || !aligned(out) || !separate(abi,sizeof(*abi),out,sizeof(*out)) || out->registry_instance || out->handle_serial)return result(ERR_INVALID_PARAMETER,2);
 MutexLock lock(mutex);View *view=nullptr;
 Error error=resolve_borrow_locked(frame.registry_instance,frame.handle_serial,ViewDomain::SCENE_READY,view);
 if(error!=OK)return result(error,14);
 auto &record=records[view->record];
 if(!RegistrationContext::current || RegistrationContext::current->level_phase!=LevelPhase::SCENE_READY || record.scene_ready_state!=Record::SceneReadyState::IN_FRAME)return result(ERR_UNAUTHORIZED,14);
 if(record.owner_thread!=Thread::get_caller_id())return result(ERR_UNAUTHORIZED,15);
 if(record.guard_enrollment!=GuardEnrollment::QUALIFIED || record.quiescing || record.close_started)return result(ERR_UNAUTHORIZED,16);
 if(record.scene_ready_flavor==Record::SceneReadyFlavor::V2 &&
   (!(record.scene_install_plan.selected_features&GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2) || !scene_install_plan_matches_locked(record)))return {ERR_UNAVAILABLE,GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1,1,0};

 DeviceRef *c=nullptr,*s=nullptr;
 error=device_ref_locked(context.registry_instance,context.handle_serial,false,c);if(error!=OK)return result(error,9);
 error=device_ref_locked(session.registry_instance,session.handle_serial,true,s);if(error!=OK)return result(error,9);
 if(c->library!=view->record || s->library!=view->record || c->device!=s->device || !c->qualification_main || !s->qualification_main)return result(ERR_UNAUTHORIZED,16);
 auto &device=device_records[c->device]; auto *rd=Object::cast_to<RenderingDevice>(device.object);
 if(!rd || !rd->is_main_instance || rd!=RenderingDevice::get_singleton() || !device.live || device.execution_thread!=rd->render_thread_id)return result(ERR_UNAVAILABLE,18);
 HolderRetirement *holder=nullptr;
 if(code.registry_instance!=instance || !code.handle_serial)return result(ERR_INVALID_DATA,9);
 for(auto *h=record.holders;h;h=h->record_next)if(h->typed_code_hold && h->handle_serial==code.handle_serial && !h->pending) { holder=h;break; }
 if(!holder || holder->generation!=record.snapshot.library_generation || holder->acquired_thread!=record.owner_thread)return result(ERR_UNAUTHORIZED,16);
 bool terminal=false;
 for(const auto &level:record.levels)if(level.binding_action.serial && level.binding_action.serial==holder->terminal_action && !level.binding_action.returned && level.binding_action.generation==holder->generation && level.binding_action.owner_thread==record.owner_thread)terminal=true;
 if(!terminal)return result(ERR_UNAUTHORIZED,16);
 for(auto *bridge:qualification_bridges)if(bridge && bridge->library==view->record && bridge->device==c->device)return result(ERR_ALREADY_IN_USE,16);
 uint32_t slot=INVALID_SLOT;for(uint32_t i=0;i<RECORD_CAPACITY;++i)if(!qualification_bridges[i]){slot=i;break;}
 if(slot==INVALID_SLOT || record.private_refs>=private_limit || qualification_serials.exhausted || holder->qualification_children==UINT32_MAX)return result(ERR_PARAMETER_RANGE_ERROR,19);
 auto *bridge=memnew(QualificationBridge);
 qualification_serials.take(bridge->serial);bridge->library=view->record;bridge->device=c->device;bridge->epoch=device.epoch;bridge->holder=holder;
 ++record.private_refs;++device.refs;++holder->qualification_children;qualification_bridges[slot]=bridge;
 *out={instance,bridge->serial};return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_backing(GDExtensionQualificationBridgeRefV1 ref,uint64_t rid,GDExtensionQualificationBackingV1 *out) {
 if(!aligned(out))return result(ERR_INVALID_PARAMETER,2);
 *out={};MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;
 Error error=qualification_bridge_locked(ref,true,true,bridge,rd,1);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
 auto *buffer=rd->storage_buffer_owner.get_or_null(RID::from_uint64(rid));
 if(!buffer || !buffer->backing_serial)return result(ERR_DOES_NOT_EXIST,7);
 *out={{1,sizeof(GDExtensionDeviceExitAbi),0},rid,buffer->backing_serial,buffer->size,uint64_t(buffer->usage)};
 return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_close(GDExtensionQualificationBridgeRefV1 ref) {
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;
 Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:17);
 bridge->closing=true;for(auto *ticket:bridge->tickets)if(ticket)ticket->admission_closed=true;if(bridge->children)return result(ERR_BUSY,16);
 auto library=bridge->library,device=bridge->device;
 CRASH_COND(!bridge->holder->qualification_children || !records[library].private_refs || !device_records[device].refs);
 --bridge->holder->qualification_children;--records[library].private_refs;--device_records[device].refs;
 for(auto *&entry:qualification_bridges)if(entry==bridge){entry=nullptr;break;}
 for(auto *child:bridge->storage)if(child){CRASH_COND(!child->acknowledged);memdelete(child);}
 for(auto *child:bridge->units)if(child){CRASH_COND(!child->acknowledged);memdelete(child);}
 for(auto *ticket:bridge->tickets)if(ticket){CRASH_COND(!ticket->acknowledged || ticket->frame_refs);memdelete(ticket);}
 if(bridge->rt){CRASH_COND(bridge->rt->occupied || bridge->rt->queued);memdelete(bridge->rt);}
 memdelete(bridge);reclaim_device_locked(device);reclaim(library);try_finalize();return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_storage_create(GDExtensionQualificationBridgeRefV1 ref,const GDExtensionStorageCreateRequestV1 *request,GDExtensionStorageBackingV1 *out) {
 if(!aligned(out) || !aligned(request) || !separate(request,sizeof(*request),out,sizeof(*out)))return result(ERR_INVALID_PARAMETER,2);
 *out={};
 if(!valid_abi(&request->abi) || request->reserved || !request->bytes || request->bytes>268435456 || !request->allocation_generation || !request->create_attempt)return result(ERR_INVALID_PARAMETER,2);
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationBridge::Storage *child=nullptr;uint32_t slot=0;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,false,bridge,rd);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
  const bool accepted=qualification_accepted_call_locked(bridge,2);
  for(auto *existing:bridge->storage)if(existing && existing->origin.allocation_slot==request->allocation_slot && existing->origin.allocation_generation==request->allocation_generation && existing->origin.part==request->part && existing->origin.create_attempt==request->create_attempt) {
   if(memcmp(&existing->origin,request,sizeof(*request)))return result(ERR_INVALID_DATA,16);
   if(existing->acknowledged)return result(ERR_DOES_NOT_EXIST,17);
   if(existing->creating)return result(ERR_BUSY,16);
   *out=existing->backing;return result(OK);
  }
  if((bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started) && !accepted)return result(ERR_BUSY,16);
  for(;slot<QualificationBridge::CHILD_CAPACITY && bridge->storage[slot];++slot){}
  if(slot==QualificationBridge::CHILD_CAPACITY || records[bridge->library].private_refs>=private_limit || qualification_serials.exhausted)return result(ERR_PARAMETER_RANGE_ERROR,19);
  if(rd->_recording_error()!=OK)return result(rd->_recording_error(),16);
  child=memnew(QualificationBridge::Storage);qualification_serials.take(child->serial);child->origin=*request;
  child->physical.bridge=bridge->serial;child->physical.child=child->serial;
  bridge->storage[slot]=child;++bridge->children;++records[bridge->library].private_refs;
 }
 // The Registry child pins the original CodeHolder throughout the driver call;
 // do not hold Registry::mutex across callbacks into original device-loss code.
 RID rid=rd->_qualification_storage_create(uint32_t(request->bytes),&child->physical);
 MutexLock lock(mutex);
 if(rid.is_null()) {
  bridge->storage[slot]=nullptr;--bridge->children;--records[bridge->library].private_refs;memdelete(child);
  return result(FAILED,16);
 }
 auto *buffer=rd->storage_buffer_owner.get_or_null(rid);CRASH_COND(!buffer || buffer->qualification_return!=&child->physical);
 child->backing={{1,sizeof(GDExtensionDeviceExitAbi),0},{instance,child->serial},rid.get_id(),buffer->backing_serial,buffer->size,uint64_t(buffer->usage),OK,1};
 child->creating=false;*out=child->backing;return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_native_unit_create(GDExtensionQualificationBridgeRefV1 ref,const GDExtensionNativeUnitCreateRequestV1 *request,const void *bytes,uint64_t size,GDExtensionNativeUnitBackingV1 *out) {
 if(!aligned(out) || !aligned(request) || !separate(request,sizeof(*request),out,sizeof(*out)))return result(ERR_INVALID_PARAMETER,2);
 *out={};
 if(!valid_abi(&request->abi) || request->reserved32 || request->reserved64 || !request->library_generation || !request->preparation_generation || !request->operation_serial || !size || size>67108864 || size!=request->container_byte_count || !bytes || !separate(bytes,size,out,sizeof(*out)) || !separate(bytes,size,request,sizeof(*request)))return result(ERR_INVALID_PARAMETER,2);
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,false,bridge,rd);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
  if(request->library_generation!=records[bridge->library].snapshot.library_generation)return result(ERR_INVALID_DATA,16);
  if(!ShaderCompilePolicy::flags_valid(request->expected_flags,rd->driver->get_shader_container_format().supported_compile_flags()))return result(ERR_UNAVAILABLE,18);
 }
 Vector<uint8_t> source;source.resize(size);memcpy(source.ptrw(),bytes,size);
 uint8_t digest[32];Error digest_error=CryptoCore::sha256(source.ptr(),size,digest);if(digest_error!=OK)return result(digest_error,16);
 if(memcmp(digest,request->container_digest,32))return result(ERR_INVALID_DATA,16);
 // Validate the actual container before the first native allocation.
 Ref<RenderingShaderContainer> parsed=rd->driver->get_shader_container_format().create_container();
 if(parsed.is_null() || !parsed->from_bytes(source))return result(ERR_INVALID_DATA,16);
 if(uint64_t(parsed->get_compile_flags())!=request->expected_flags || parsed->get_shader_reflection().pipeline_type!=RenderingDevice::PIPELINE_TYPE_COMPUTE)return result(ERR_INVALID_DATA,16);
 QualificationBridge::NativeUnit *child=nullptr;uint32_t slot=0;RID shader_reservation,pipeline_reservation;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,false,bridge,rd);if(error!=OK)return result(error,16);
  const bool accepted=qualification_accepted_call_locked(bridge,4);
  for(auto *existing:bridge->units)if(existing && existing->origin.record_slot==request->record_slot && existing->origin.preparation_generation==request->preparation_generation && existing->origin.operation_serial==request->operation_serial && existing->origin.physical_unit==request->physical_unit) {
   if(memcmp(&existing->origin,request,sizeof(*request)))return result(ERR_INVALID_DATA,16);
   if(existing->acknowledged)return result(ERR_DOES_NOT_EXIST,17);
   if(existing->creating)return result(ERR_BUSY,16);
   *out=existing->backing;return result(OK);
  }
  if((bridge->closing || records[bridge->library].quiescing || records[bridge->library].close_started) && !accepted)return result(ERR_BUSY,16);
  for(;slot<QualificationBridge::CHILD_CAPACITY && bridge->units[slot];++slot){}
  if(slot==QualificationBridge::CHILD_CAPACITY || records[bridge->library].private_refs>=private_limit || qualification_serials.exhausted || !rd->next_qualification_free_event || !rd->next_qualification_object_serial || rd->next_qualification_object_serial==UINT64_MAX)return result(ERR_PARAMETER_RANGE_ERROR,19);
  if(rd->_recording_error()!=OK)return result(rd->_recording_error(),16);
  child=memnew(QualificationBridge::NativeUnit);qualification_serials.take(child->serial);child->origin=*request;
  child->shader.bridge=child->pipeline.bridge=bridge->serial;child->shader.child=child->pipeline.child=child->serial;
  child->shader.serial=rd->next_qualification_object_serial++;child->pipeline.serial=rd->next_qualification_object_serial++;
  child->backing.abi={1,sizeof(GDExtensionDeviceExitAbi),0};child->backing.unit={instance,child->serial};child->backing.event_seq=rd->next_qualification_free_event++;
  shader_reservation=rd->shader_owner.allocate_rid();pipeline_reservation=rd->compute_pipeline_owner.allocate_rid();
  if(shader_reservation.is_null() || pipeline_reservation.is_null()) {
   if(shader_reservation.is_valid()){rd->shader_owner.initialize_rid(shader_reservation,RenderingDevice::Shader());rd->shader_owner.free(shader_reservation);}
   if(pipeline_reservation.is_valid()){rd->compute_pipeline_owner.initialize_rid(pipeline_reservation,RenderingDevice::ComputePipeline());rd->compute_pipeline_owner.free(pipeline_reservation);}
   memdelete(child);return result(ERR_CANT_CREATE,19);
  }
  bridge->units[slot]=child;++bridge->children;++records[bridge->library].private_refs;
 }
 RID shader=rd->_qualification_shader_create(source,RID(),{},&child->shader,shader_reservation);
 if(shader.is_null()) {
  rd->shader_owner.initialize_rid(shader_reservation,RenderingDevice::Shader());rd->shader_owner.free(shader_reservation);
  rd->compute_pipeline_owner.initialize_rid(pipeline_reservation,RenderingDevice::ComputePipeline());rd->compute_pipeline_owner.free(pipeline_reservation);
  MutexLock lock(mutex);bridge->units[slot]=nullptr;--bridge->children;--records[bridge->library].private_refs;memdelete(child);return result(FAILED,16);
 }
 child->shader.rid=shader.get_id();child->backing.shader_rid=shader.get_id();child->backing.shader_serial=child->shader.serial;child->backing.known_mask=1;
 child->backing.effective_flags=uint64_t(rd->shader_get_compile_flags(shader));
 RID pipeline;
 if(child->backing.effective_flags!=request->expected_flags) {
  child->backing.state=2;child->backing.phase=1;child->backing.native_error=ERR_INVALID_DATA;
 } else {
  pipeline=rd->_qualification_pipeline_create(shader,{},&child->pipeline,pipeline_reservation);
  if(pipeline.is_null()) { child->backing.state=2;child->backing.phase=2;child->backing.native_error=FAILED; }
 }
 if(pipeline.is_valid()) {
  child->pipeline.rid=pipeline.get_id();child->backing.pipeline_rid=pipeline.get_id();child->backing.pipeline_serial=child->pipeline.serial;child->backing.known_mask=3;child->backing.state=1;
 } else {
  rd->compute_pipeline_owner.initialize_rid(pipeline_reservation,RenderingDevice::ComputePipeline());rd->compute_pipeline_owner.free(pipeline_reservation);
  child->pipeline.serial=0;
 }
 MutexLock lock(mutex);child->creating=false;*out=child->backing;return result(OK);
}
void Registry::qualification_retire_step(QualificationBridge *bridge,RenderingDevice *rd,uint64_t serial,uint32_t kind) {
 if(rd->_recording_error()!=OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks() || !rd->_thread_safe_.try_lock())return;
 struct Unlock { Mutex &mutex;~Unlock(){mutex.unlock();} } unlock{rd->_thread_safe_};
 auto no_dependents=[&](RID rid) { auto *set=rd->dependency_map.getptr(rid);return !set || set->is_empty(); };
 auto enqueue=[&](QualificationPhysicalReturn &cell,uint32_t type)->uint32_t {
  if(!cell.rid || cell.enqueued)return 0;
  for(auto *ticket:cell.uses)if(!ticket->physically_retired.load())return 3;
  if(cell.dependent_children.load() || !no_dependents(RID::from_uint64(cell.rid)))return 4;
  if(type==1) {
   auto *buffer=rd->storage_buffer_owner.get_or_null(RID::from_uint64(cell.rid));
   if(!buffer || buffer->qualification_return!=&cell || buffer->backing_serial!=cell.serial)return 6;
   // Marked empty-create buffers never enter a legacy asynchronous worker.
   // A checked upload owns any later worker through the original ticket set.
   if(buffer->transfer_worker_index>=0)return 3;
  } else if(type==2) {
   auto *shader=rd->shader_owner.get_or_null(RID::from_uint64(cell.rid));
   if(!shader || shader->qualification_return!=&cell)return 6;
   if(!rd->driver->shader_retire_ready_nonblocking(shader->driver_id))return 2;
  } else {
   auto *pipeline=rd->compute_pipeline_owner.get_or_null(RID::from_uint64(cell.rid));
   if(!pipeline || pipeline->qualification_return!=&cell)return 6;
  }
  if(!rd->next_qualification_free_event || !rd->frames[rd->frame].qualification_epoch)return 5;
  // Revalidate the original read-only safe window immediately before each
  // physical enqueue, including the synchronous retire entrypoint.
  if(rd->_recording_error()!=OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks())return 1;
  rd->qualification_retiring=true;
  rd->free_rid(RID::from_uint64(cell.rid));
  rd->qualification_retiring=false;
  return cell.enqueued ? 0 : 5;
 };
 if(kind==1) {
  QualificationBridge::Storage *child=nullptr;for(auto *entry:bridge->storage)if(entry && entry->serial==serial){child=entry;break;}
  if(!child || !child->retiring)return;
  bool failed=false;{MutexLock lock(mutex);failed=child->maintenance_error!=OK;}
  uint32_t reason=failed?0:enqueue(child->physical,1);
  MutexLock lock(mutex);MutexLock physical_lock(child->physical.mutex);
  auto &report=child->report;const auto &physical=child->physical;
  report.frame_slot=physical.frame_slot;report.frame_epoch=physical.frame_epoch;report.event_seq=physical.free_event;
  if(physical.enqueued)report.dependency_set_serial=child->dependency_serial;
  if(failed) { report.state=2;report.return_status=0;report.reason=5;report.native_error=child->maintenance_error; }
  else if(physical.free_event) { report.state=1;report.return_status=1;report.reason=0; }
  else if(reason>=5) { report.state=2;report.reason=reason; }
  else report.reason=reason ? reason : 1;
 } else if(kind==2) {
  QualificationBridge::NativeUnit *child=nullptr;for(auto *entry:bridge->units)if(entry && entry->serial==serial){child=entry;break;}
  if(!child || !child->retiring)return;
  bool failed=false;{MutexLock lock(mutex);if(child->report.report_revision==UINT64_MAX)return;failed=child->maintenance_error!=OK;}
  uint32_t reason=failed?0:enqueue(child->pipeline,3);
  // Pipeline queueing removes its live dependency edge. The original frame
  // disposer still executes pipeline free before shader free in that frame.
  uint32_t shader_reason=failed?0:(child->pipeline.enqueued && !child->pipeline.free_event && child->pipeline.frame_slot!=uint32_t(rd->frame)) ? 4 : enqueue(child->shader,2);if(shader_reason)reason=shader_reason;
  MutexLock lock(mutex);MutexLock shader_lock(child->shader.mutex);MutexLock pipeline_lock(child->pipeline.mutex);
  auto report=child->report;
  const auto &shader=child->shader;const auto &pipeline=child->pipeline;
  report.enqueued_mask=(shader.enqueued?1U:0U)|(pipeline.enqueued?2U:0U);
  report.freed_mask=(shader.free_event?1U:0U)|(pipeline.free_event?2U:0U);
  report.shader_frame_slot=shader.rid?shader.frame_slot:0;report.pipeline_frame_slot=pipeline.rid?pipeline.frame_slot:0;
  report.shader_frame_epoch=shader.frame_epoch;report.pipeline_frame_epoch=pipeline.frame_epoch;
  report.shader_free_event_seq=shader.free_event;report.pipeline_free_event_seq=pipeline.free_event;
  report.unresolved_mask=child->backing.possible_mask;
  if(report.enqueued_mask==child->backing.known_mask && !report.unresolved_mask)report.dependency_set_serial=child->dependency_serial;
  if(failed) { report.state=2;report.return_status=0;report.reason=5;report.native_error=child->maintenance_error; }
  else if(report.freed_mask==child->backing.known_mask && !report.unresolved_mask) { report.state=1;report.return_status=1;report.reason=0; }
  else if(reason>=5 || report.unresolved_mask) { report.state=2;report.reason=reason==6?6:5; }
  else report.reason=reason?reason:1;
  if(memcmp(&report,&child->report,sizeof(report))) {
   if(report.report_revision==UINT64_MAX) { report.state=2;report.return_status=0;report.reason=5; }
   else ++report.report_revision;
   child->report=report;
  }
 }
}

Error Registry::qualification_schedule(GDExtensionQualificationBridgeRefV1 ref,uint64_t serial,uint32_t kind,const GDExtensionQualificationBudgetV1 &budget) {
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;bool *queued=nullptr;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return error;
  if(kind==1)for(auto *child:bridge->storage)if(child && child->serial==serial && child->retiring){if(child->report.state==1)return OK;queued=&child->queued;break;}
  if(kind==2)for(auto *child:bridge->units)if(child && child->serial==serial && child->retiring){if(child->report.state==1)return OK;queued=&child->queued;break;}
  if(kind==3)for(auto *ticket:bridge->tickets)if(ticket && !ticket->acknowledged && ticket->serial==serial){if(ticket->physically_retired.load() && !ticket->continuation_pending)return OK;queued=&ticket->queued;break;}
  if(!queued)return ERR_DOES_NOT_EXIST;
  if(*queued)return OK;
  if(records[bridge->library].private_refs>=private_limit)return OK;
  *queued=true;++bridge->children;++records[bridge->library].private_refs;
 }
 auto *server=Object::cast_to<RenderingServerDefault>(RenderingServer::get_singleton());
 if(server && server->_try_qualification_service(bridge->serial,serial,kind,budget.max_passes,budget.max_micros))return OK;
 // A refused queue did not publish a command. Undo this one retained queue pin,
 // while preserving all original native child and possible-prefix obligations.
 MutexLock lock(mutex);CRASH_COND(!*queued);*queued=false;--bridge->children;--records[bridge->library].private_refs;return OK;
}

void Registry::qualification_service(uint64_t bridge_serial,uint64_t serial,uint32_t kind,uint32_t passes,uint32_t micros) {
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;bool *queued=nullptr;Error admitted=OK;
 {
  MutexLock lock(mutex);
  for(auto *entry:qualification_bridges)if(entry && entry->serial==bridge_serial){bridge=entry;break;}
  CRASH_COND(!bridge);
  if(kind==1)for(auto *child:bridge->storage)if(child && child->serial==serial){queued=&child->queued;break;}
  if(kind==2)for(auto *child:bridge->units)if(child && child->serial==serial){queued=&child->queued;break;}
  if(kind==3)for(auto *ticket:bridge->tickets)if(ticket && ticket->serial==serial){queued=&ticket->queued;break;}
  CRASH_COND(!queued || !*queued);
  admitted=qualification_bridge_locked({instance,bridge_serial},true,false,bridge,rd);
 }
 if(admitted==OK) {
  const uint64_t start=OS::get_singleton()->get_ticks_usec();
  for(uint32_t pass=0;pass<passes;++pass) {
   if(OS::get_singleton()->get_ticks_usec()-start>=micros)break;
   if(kind==3){for(auto *ticket:bridge->tickets)if(ticket && ticket->serial==serial)qualification_ticket_step(ticket,rd);}
   else qualification_retire_step(bridge,rd,serial,kind);
   bool returned=false;
   { MutexLock lock(mutex);
    if(kind==3)for(auto *ticket:bridge->tickets)if(ticket && ticket->serial==serial)returned=ticket->physically_retired.load() && !ticket->continuation_pending;
    if(kind==1)for(auto *child:bridge->storage)if(child && child->serial==serial)returned=child->report.state==1;
    if(kind==2)for(auto *child:bridge->units)if(child && child->serial==serial)returned=child->report.state==1;
   }
   if(returned)break;
   const Error error=rd->_qualification_maintenance();
   // Publish a real maintenance failure before any later retirement step can
   // enqueue another driver free. Keep the first cause and the original child.
   if(error!=OK && error!=ERR_BUSY && kind!=3) {
    MutexLock lock(mutex);
    if(kind==1)for(auto *child:bridge->storage)if(child && child->serial==serial && child->report.state!=1 && child->maintenance_error==OK) {
     child->maintenance_error=error;child->report.state=2;child->report.return_status=0;child->report.reason=5;child->report.native_error=error;
    }
    if(kind==2)for(auto *child:bridge->units)if(child && child->serial==serial && child->report.state!=1 && child->maintenance_error==OK) {
     child->maintenance_error=error;
     if(child->report.report_revision!=UINT64_MAX) {child->report.state=2;child->report.return_status=0;child->report.reason=5;child->report.native_error=error;++child->report.report_revision;}
    }
   }
   if(kind==3){for(auto *ticket:bridge->tickets)if(ticket && ticket->serial==serial)qualification_ticket_step(ticket,rd);}
   else qualification_retire_step(bridge,rd,serial,kind);
   if(error!=OK && error!=ERR_BUSY) {
    MutexLock lock(mutex);
    if(kind==3)for(auto *ticket:bridge->tickets)if(ticket && ticket->serial==serial && !ticket->physically_retired.load()) {
     ticket->append_failed=true;ticket->report.native_error=error;qualification_ticket_publish(ticket,rd);
    }
   }
   if(error!=OK)break;
  }
 }
 MutexLock lock(mutex);CRASH_COND(!*queued);*queued=false;--bridge->children;--records[bridge->library].private_refs;
}

Error Registry::qualification_cleanup_retire_matches_original_child_locked(QualificationBridge *bridge,uint32_t kind,const void *bytes) {
 // Read only under the original Registry lock. This authenticates existing
 // cleanup ownership; it neither reserves a retirement nor changes a child.
 if(kind==3) {
  GDExtensionStorageRetireRequestV1 request;memcpy(&request,bytes,sizeof(request));
  QualificationBridge::Storage *child=nullptr;
  if(request.backing.registry_instance==instance)for(auto *entry:bridge->storage)if(entry && entry->serial==request.backing.handle_serial){child=entry;break;}
  if(!child || child->creating || child->acknowledged)return ERR_DOES_NOT_EXIST;
  if(child->retiring)return memcmp(&child->retirement,&request,sizeof(request))?ERR_INVALID_DATA:OK;
  if(child->backing.rid!=request.rid || child->backing.backing_serial!=request.observed_backing_serial || child->origin.allocation_slot!=request.allocation_slot || child->origin.part!=request.part || child->origin.allocation_generation!=request.allocation_generation)return ERR_INVALID_DATA;
  return OK;
 }
 if(kind==5) {
  GDExtensionNativeUnitRetireRequestV1 request;memcpy(&request,bytes,sizeof(request));
  QualificationBridge::NativeUnit *child=nullptr;
  if(request.unit.registry_instance==instance)for(auto *entry:bridge->units)if(entry && entry->serial==request.unit.handle_serial){child=entry;break;}
  if(!child || child->creating || child->acknowledged)return ERR_DOES_NOT_EXIST;
  if(child->retiring && child->report.retire_operation!=request.retire_operation)return ERR_INVALID_DATA;
  return OK;
 }
 return ERR_INVALID_PARAMETER;
}

GDExtensionDeviceSessionResultV1 Registry::qualification_storage_retire(GDExtensionQualificationBridgeRefV1 ref,const GDExtensionStorageRetireRequestV1 *request,GDExtensionStorageRetireRefV1 *out,GDExtensionStorageRetireReportV1 *report) {
 if(!aligned(request)||!aligned(out)||!aligned(report)||!separate(request,sizeof(*request),out,sizeof(*out))||!separate(request,sizeof(*request),report,sizeof(*report))||!separate(out,sizeof(*out),report,sizeof(*report)))return result(ERR_INVALID_PARAMETER,2);
 *out={};*report={};if(!valid_abi(&request->abi)||request->reserved||!request->retire_attempt)return result(ERR_INVALID_PARAMETER,2);
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationBridge::Storage *child=nullptr;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,false,bridge,rd);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
  error=qualification_cleanup_retire_matches_original_child_locked(bridge,3,request);if(error!=OK)return result(error,error==ERR_DOES_NOT_EXIST?17:16);
  if(request->backing.registry_instance==instance)for(auto *entry:bridge->storage)if(entry && entry->serial==request->backing.handle_serial){child=entry;break;}
  if(child->retiring) {
   *out={instance,child->retire_serial};*report=child->report;return result(OK);
  }
  if(qualification_serials.exhausted || qualification_serials.next==UINT64_MAX)return result(ERR_PARAMETER_RANGE_ERROR,19);
  qualification_serials.take(child->retire_serial);qualification_serials.take(child->dependency_serial);child->retirement=*request;child->retiring=true;child->physical.use_closed.store(true);
  auto &r=child->report;r.abi={1,sizeof(GDExtensionDeviceExitAbi),0};r.rid=request->rid;r.backing_serial=request->observed_backing_serial;r.retire_cell_serial=child->retire_serial;r.frame_slot=UINT32_MAX;r.allocation_generation=request->allocation_generation;r.retire_attempt=request->retire_attempt;r.allocation_slot=request->allocation_slot;r.part=request->part;
 }
 qualification_retire_step(bridge,rd,child->serial,1);
 MutexLock lock(mutex);*out={instance,child->retire_serial};*report=child->report;return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_native_unit_retire(GDExtensionQualificationBridgeRefV1 ref,const GDExtensionNativeUnitRetireRequestV1 *request,GDExtensionNativeUnitRetireRefV1 *out,GDExtensionNativeUnitRetireReportV1 *report) {
 if(!aligned(request)||!aligned(out)||!aligned(report)||!separate(request,sizeof(*request),out,sizeof(*out))||!separate(request,sizeof(*request),report,sizeof(*report))||!separate(out,sizeof(*out),report,sizeof(*report)))return result(ERR_INVALID_PARAMETER,2);
 *out={};*report={};if(!valid_abi(&request->abi)||request->reserved||!request->retire_operation)return result(ERR_INVALID_PARAMETER,2);
 QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;QualificationBridge::NativeUnit *child=nullptr;
 {
  MutexLock lock(mutex);Error error=qualification_bridge_locked(ref,true,false,bridge,rd);if(error!=OK)return result(error,error==ERR_UNAUTHORIZED?15:16);
  error=qualification_cleanup_retire_matches_original_child_locked(bridge,5,request);if(error!=OK)return result(error,error==ERR_DOES_NOT_EXIST?17:16);
  if(request->unit.registry_instance==instance)for(auto *entry:bridge->units)if(entry && entry->serial==request->unit.handle_serial){child=entry;break;}
  if(child->retiring) {
   *out={instance,child->retire_serial};*report=child->report;return result(OK);
  }
  if(qualification_serials.exhausted || qualification_serials.next==UINT64_MAX)return result(ERR_PARAMETER_RANGE_ERROR,19);
  qualification_serials.take(child->retire_serial);qualification_serials.take(child->dependency_serial);child->retiring=true;child->shader.use_closed.store(true);child->pipeline.use_closed.store(true);
  auto &r=child->report;r.abi={1,sizeof(GDExtensionDeviceExitAbi),0};r.unit=request->unit;r.shader_rid=child->backing.shader_rid;r.pipeline_rid=child->backing.pipeline_rid;r.shader_serial=child->backing.shader_serial;r.pipeline_serial=child->backing.pipeline_serial;r.shader_frame_slot=r.shader_rid?UINT32_MAX:0;r.pipeline_frame_slot=r.pipeline_rid?UINT32_MAX:0;r.report_revision=1;r.retire_operation=request->retire_operation;
 }
 qualification_retire_step(bridge,rd,child->serial,2);
 MutexLock lock(mutex);*out={instance,child->retire_serial};*report=child->report;return result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::qualification_storage_retire_observe(GDExtensionQualificationBridgeRefV1 ref,GDExtensionStorageRetireRefV1 retire,const GDExtensionQualificationBudgetV1 *budget,GDExtensionStorageRetireReportV1 *out) {
 if(!aligned(budget)||!aligned(out)||!separate(budget,sizeof(*budget),out,sizeof(*out)))return result(ERR_INVALID_PARAMETER,2);
 *out={};if(!valid_abi(&budget->abi)||budget->reserved||budget->max_passes<1||budget->max_passes>32||budget->max_micros<1||budget->max_micros>5000)return result(ERR_INVALID_PARAMETER,2);
 uint64_t serial=0;
 { MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);
  if(retire.registry_instance==instance)for(auto *child:bridge->storage)if(child && !child->acknowledged && child->retire_serial && child->retire_serial==retire.handle_serial){serial=child->serial;break;}
  if(!serial)return result(ERR_DOES_NOT_EXIST,17);
 }
 Error error=qualification_schedule(ref,serial,1,*budget);if(error!=OK)return result(error,17);
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);
 for(auto *child:bridge->storage)if(child && child->serial==serial){*out=child->report;return result(OK);}return result(ERR_DOES_NOT_EXIST,17);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_native_unit_retire_observe(GDExtensionQualificationBridgeRefV1 ref,GDExtensionNativeUnitRetireRefV1 retire,const GDExtensionQualificationBudgetV1 *budget,GDExtensionNativeUnitRetireReportV1 *out) {
 if(!aligned(budget)||!aligned(out)||!separate(budget,sizeof(*budget),out,sizeof(*out)))return result(ERR_INVALID_PARAMETER,2);
 *out={};if(!valid_abi(&budget->abi)||budget->reserved||budget->max_passes<1||budget->max_passes>32||budget->max_micros<1||budget->max_micros>5000)return result(ERR_INVALID_PARAMETER,2);
 uint64_t serial=0;
 { MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);
  if(retire.registry_instance==instance)for(auto *child:bridge->units)if(child && !child->acknowledged && child->retire_serial && child->retire_serial==retire.handle_serial){serial=child->serial;break;}
  if(!serial)return result(ERR_DOES_NOT_EXIST,17);
 }
 Error error=qualification_schedule(ref,serial,2,*budget);if(error!=OK)return result(error,17);
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);
 for(auto *child:bridge->units)if(child && child->serial==serial){*out=child->report;return result(OK);}return result(ERR_DOES_NOT_EXIST,17);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_storage_retire_release(GDExtensionQualificationBridgeRefV1 ref,GDExtensionStorageRetireRefV1 retire) {
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);
 if(retire.registry_instance!=instance || !retire.handle_serial)return result(ERR_INVALID_DATA,17);
 for(auto *&child:bridge->storage)if(child && !child->acknowledged && child->retire_serial==retire.handle_serial) {
  if(child->maintenance_error!=OK || child->report.state!=1 || child->report.return_status!=1 || child->report.reason || child->report.native_error || child->report.native_code || !child->physical.free_event)return result(ERR_BUSY,16);
  if(child->queued)return result(ERR_BUSY,19);
  child->acknowledged=true;--bridge->children;--records[bridge->library].private_refs;return result(OK);
 }
 return result(ERR_DOES_NOT_EXIST,17);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_native_unit_retire_release(GDExtensionQualificationBridgeRefV1 ref,GDExtensionNativeUnitRetireRefV1 retire) {
 MutexLock lock(mutex);QualificationBridge *bridge=nullptr;RenderingDevice *rd=nullptr;Error error=qualification_bridge_locked(ref,false,false,bridge,rd);if(error!=OK)return result(error,17);
 if(retire.registry_instance!=instance || !retire.handle_serial)return result(ERR_INVALID_DATA,17);
 for(auto *&child:bridge->units)if(child && !child->acknowledged && child->retire_serial==retire.handle_serial) {
  if(child->maintenance_error!=OK || child->report.state!=1 || child->report.return_status!=1 || child->report.reason || child->report.native_error || child->report.native_code || child->report.unresolved_mask || child->report.freed_mask!=child->backing.known_mask || child->backing.possible_mask)return result(ERR_BUSY,16);
  if(child->queued)return result(ERR_BUSY,19);
  const auto &backing=child->backing;const auto &report=child->report;
  const uint32_t known=backing.known_mask;
  if(!child->retiring || (known!=1U && known!=3U) || !valid_abi(&backing.abi) || !valid_abi(&report.abi) ||
    backing.unit.registry_instance!=instance || backing.unit.handle_serial!=child->serial ||
    report.unit.registry_instance!=backing.unit.registry_instance || report.unit.handle_serial!=backing.unit.handle_serial ||
    !backing.event_seq || !report.report_revision || !report.retire_operation || !child->dependency_serial ||
    report.dependency_set_serial!=child->dependency_serial || report.enqueued_mask!=known || report.absent_mask)return result(ERR_BUSY,16);
  MutexLock shader_lock(child->shader.mutex);MutexLock pipeline_lock(child->pipeline.mutex);
  auto matches=[&](const QualificationPhysicalReturn &cell,uint64_t rid,uint64_t serial,uint64_t reported_rid,uint64_t reported_serial,uint32_t frame,uint64_t epoch,uint64_t event) {
   return rid && serial && cell.bridge==bridge->serial && cell.child==child->serial && cell.rid==rid && cell.serial==serial &&
     cell.use_closed.load() && cell.enqueued && cell.frame_slot!=UINT32_MAX && cell.frame_epoch && cell.free_event &&
     cell.free_event==cell.reserved_event && cell.free_event>backing.event_seq && reported_rid==rid && reported_serial==serial &&
     frame==cell.frame_slot && epoch==cell.frame_epoch && event==cell.free_event;
  };
  auto absent=[](const QualificationPhysicalReturn &cell,uint64_t rid,uint64_t serial,uint64_t reported_rid,uint64_t reported_serial,uint32_t frame,uint64_t epoch,uint64_t event) {
   return !rid && !serial && !reported_rid && !reported_serial && !frame && !epoch && !event &&
     !cell.rid && !cell.serial && !cell.enqueued && !cell.reserved_event && !cell.free_event;
  };
  if((known&1U)?!matches(child->shader,backing.shader_rid,backing.shader_serial,report.shader_rid,report.shader_serial,report.shader_frame_slot,report.shader_frame_epoch,report.shader_free_event_seq):
     !absent(child->shader,backing.shader_rid,backing.shader_serial,report.shader_rid,report.shader_serial,report.shader_frame_slot,report.shader_frame_epoch,report.shader_free_event_seq))return result(ERR_BUSY,16);
  if((known&2U)?!matches(child->pipeline,backing.pipeline_rid,backing.pipeline_serial,report.pipeline_rid,report.pipeline_serial,report.pipeline_frame_slot,report.pipeline_frame_epoch,report.pipeline_free_event_seq):
     !absent(child->pipeline,backing.pipeline_rid,backing.pipeline_serial,report.pipeline_rid,report.pipeline_serial,report.pipeline_frame_slot,report.pipeline_frame_epoch,report.pipeline_free_event_seq))return result(ERR_BUSY,16);
  if(known==3U && child->shader.free_event==child->pipeline.free_event)return result(ERR_BUSY,16);
  child->acknowledged=true;--bridge->children;--records[bridge->library].private_refs;return result(OK);
 }
 return result(ERR_DOES_NOT_EXIST,17);
}
static GDExtensionDeviceSessionResultV1 open(const GDExtensionDeviceExitAbi *a,GDExtensionDeviceExitSceneReadyContextRef f,GDExtensionDeviceRegistryContextRef c,GDExtensionOriginalDeviceSessionRef s,GDExtensionDeviceExitLibraryCodeHoldRef h,GDExtensionQualificationBridgeRefV1 *o){return Registry::get_singleton().qualification_open(a,f,c,s,h,o);}
static GDExtensionDeviceSessionResultV1 backing(GDExtensionQualificationBridgeRefV1 b,uint64_t r,GDExtensionQualificationBackingV1 *o){return Registry::get_singleton().qualification_backing(b,r,o);}
static GDExtensionDeviceSessionResultV1 close(GDExtensionQualificationBridgeRefV1 b){return Registry::get_singleton().qualification_close(b);}

static GDExtensionDeviceSessionResultV1 storage_create(GDExtensionQualificationBridgeRefV1 b,const GDExtensionStorageCreateRequestV1 *q,GDExtensionStorageBackingV1 *o){return Registry::get_singleton().qualification_storage_create(b,q,o);}
static GDExtensionDeviceSessionResultV1 storage_retire(GDExtensionQualificationBridgeRefV1 b,const GDExtensionStorageRetireRequestV1 *q,GDExtensionStorageRetireRefV1 *r,GDExtensionStorageRetireReportV1 *o){return Registry::get_singleton().qualification_storage_retire(b,q,r,o);}
static GDExtensionDeviceSessionResultV1 storage_retire_observe(GDExtensionQualificationBridgeRefV1 b,GDExtensionStorageRetireRefV1 r,const GDExtensionQualificationBudgetV1 *q,GDExtensionStorageRetireReportV1 *o){return Registry::get_singleton().qualification_storage_retire_observe(b,r,q,o);}
static GDExtensionDeviceSessionResultV1 storage_retire_release(GDExtensionQualificationBridgeRefV1 b,GDExtensionStorageRetireRefV1 r){return Registry::get_singleton().qualification_storage_retire_release(b,r);}
static GDExtensionDeviceSessionResultV1 native_unit_create(GDExtensionQualificationBridgeRefV1 b,const GDExtensionNativeUnitCreateRequestV1 *q,const void *p,uint64_t n,GDExtensionNativeUnitBackingV1 *o){return Registry::get_singleton().qualification_native_unit_create(b,q,p,n,o);}
static GDExtensionDeviceSessionResultV1 native_unit_retire(GDExtensionQualificationBridgeRefV1 b,const GDExtensionNativeUnitRetireRequestV1 *q,GDExtensionNativeUnitRetireRefV1 *r,GDExtensionNativeUnitRetireReportV1 *o){return Registry::get_singleton().qualification_native_unit_retire(b,q,r,o);}
static GDExtensionDeviceSessionResultV1 native_unit_retire_observe(GDExtensionQualificationBridgeRefV1 b,GDExtensionNativeUnitRetireRefV1 r,const GDExtensionQualificationBudgetV1 *q,GDExtensionNativeUnitRetireReportV1 *o){return Registry::get_singleton().qualification_native_unit_retire_observe(b,r,q,o);}
static GDExtensionDeviceSessionResultV1 native_unit_retire_release(GDExtensionQualificationBridgeRefV1 b,GDExtensionNativeUnitRetireRefV1 r){return Registry::get_singleton().qualification_native_unit_retire_release(b,r);}
static GDExtensionDeviceSessionResultV1 record(GDExtensionQualificationBridgeRefV1 b,const GDExtensionQualificationRecordV1 *q,GDExtensionQualificationTicketRefV1 *t,GDExtensionQualificationReportV1 *o){return Registry::get_singleton().qualification_record(b,q,t,o);}
static GDExtensionDeviceSessionResultV1 observe(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,const GDExtensionQualificationBudgetV1 *q,GDExtensionQualificationReportV1 *o){return Registry::get_singleton().qualification_observe(b,t,q,o);}
static GDExtensionDeviceSessionResultV1 facts(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,uint64_t r,uint32_t f,uint32_t c,GDExtensionQualificationFactV1 *o,uint32_t *n){return Registry::get_singleton().qualification_facts(b,t,r,f,c,o,n);}
static GDExtensionDeviceSessionResultV1 copy(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,uint64_t r,void *o,uint32_t c,uint32_t *n){return Registry::get_singleton().qualification_copy(b,t,r,o,c,n);}
static GDExtensionDeviceSessionResultV1 release(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t){return Registry::get_singleton().qualification_release(b,t);}
static GDExtensionDeviceSessionResultV1 prepare_uploads(GDExtensionQualificationBridgeRefV1 b,const GDExtensionQualificationUploadPrepareV1 *q,const GDExtensionQualificationUploadPlanV1 *p,uint32_t n,GDExtensionQualificationTicketRefV1 *t,GDExtensionQualificationReportV1 *o){return Registry::get_singleton().qualification_prepare_uploads(b,q,p,n,t,o);}
static GDExtensionDeviceSessionResultV1 upload(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,const GDExtensionQualificationUploadRequestV1 *q,const void *p,uint32_t n,GDExtensionQualificationUploadSegmentV1 *s,uint32_t c,GDExtensionQualificationUploadReportV1 *o){return Registry::get_singleton().qualification_upload(b,t,q,p,n,s,c,o);}
static GDExtensionDeviceSessionResultV1 record_prepared(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,const GDExtensionQualificationRecordV1 *q,GDExtensionQualificationReportV1 *o){return Registry::get_singleton().qualification_record_prepared(b,t,q,o);}
static GDExtensionDeviceSessionResultV1 cancel(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t){return Registry::get_singleton().qualification_cancel(b,t);}
static GDExtensionDeviceSessionResultV1 rt_configure(GDExtensionQualificationBridgeRefV1 b,const GDExtensionQualificationRtConfigV1 *c){return Registry::get_singleton().qualification_rt_configure(b,c);}
static GDExtensionDeviceSessionResultV1 rt_reserve(GDExtensionQualificationBridgeRefV1 b,const GDExtensionQualificationRtRequestV1 *h,const void *q,const void *p,GDExtensionQualificationRtRefV1 *r){return Registry::get_singleton().qualification_rt_reserve(b,h,q,p,r);}
static GDExtensionDeviceSessionResultV1 rt_activate(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationRtRefV1 r){return Registry::get_singleton().qualification_rt_activate(b,r);}
static GDExtensionDeviceSessionResultV1 rt_observe(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationRtRefV1 r,GDExtensionQualificationRtObservationV1 *o,void *p,uint32_t n,GDExtensionQualificationUploadSegmentV1 *s,uint32_t c){return Registry::get_singleton().qualification_rt_observe(b,r,o,p,n,s,c);}
static GDExtensionDeviceSessionResultV1 rt_release(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationRtRefV1 r){return Registry::get_singleton().qualification_rt_release(b,r);}
void register_qualification_interfaces() {
 GDExtension::register_interface_function("device_session_qualification_rt_configure_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&rt_configure));
 GDExtension::register_interface_function("device_session_qualification_rt_reserve_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&rt_reserve));
 GDExtension::register_interface_function("device_session_qualification_rt_activate_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&rt_activate));
 GDExtension::register_interface_function("device_session_qualification_rt_observe_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&rt_observe));
 GDExtension::register_interface_function("device_session_qualification_rt_release_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&rt_release));

 GDExtension::register_interface_function("device_session_qualification_record_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&record));
 GDExtension::register_interface_function("device_session_qualification_observe_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&observe));
 GDExtension::register_interface_function("device_session_qualification_facts_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&facts));
 GDExtension::register_interface_function("device_session_qualification_copy_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&copy));
 GDExtension::register_interface_function("device_session_qualification_release_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&release));
 GDExtension::register_interface_function("device_session_qualification_prepare_uploads_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&prepare_uploads));
 GDExtension::register_interface_function("device_session_qualification_upload_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&upload));
 GDExtension::register_interface_function("device_session_qualification_record_prepared_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&record_prepared));
 GDExtension::register_interface_function("device_session_qualification_cancel_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&cancel));

 GDExtension::register_interface_function("device_session_qualification_storage_create_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&storage_create));
 GDExtension::register_interface_function("device_session_qualification_storage_retire_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&storage_retire));
 GDExtension::register_interface_function("device_session_qualification_storage_retire_observe_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&storage_retire_observe));
 GDExtension::register_interface_function("device_session_qualification_storage_retire_release_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&storage_retire_release));
 GDExtension::register_interface_function("device_session_qualification_native_unit_create_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&native_unit_create));
 GDExtension::register_interface_function("device_session_qualification_native_unit_retire_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&native_unit_retire));
 GDExtension::register_interface_function("device_session_qualification_native_unit_retire_observe_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&native_unit_retire_observe));
 GDExtension::register_interface_function("device_session_qualification_native_unit_retire_release_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&native_unit_retire_release));
 GDExtension::register_interface_function("device_session_qualification_open_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&open));
 GDExtension::register_interface_function("device_session_qualification_backing_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&backing));
 GDExtension::register_interface_function("device_session_qualification_close_v1",reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&close));
}
}
