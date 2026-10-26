#include "gdextension_library_exit_guard.h"
#include "gdextension.h"
#include "core/object/object.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error error,uint32_t reason=0,uint64_t expected=1,uint64_t actual=0){return {int32_t(error),reason,expected,actual};}
GDExtensionDeviceSessionResultV1 reference_error(Error error,uint64_t registry,uint64_t serial,uint64_t instance){
 return result(error,!registry || !serial?4:registry!=instance?5:error==ERR_DOES_NOT_EXIST?8:9);
}
template<class T> bool aligned_pointer(const T* p){return p && uintptr_t(p)%alignof(T)==0;}
bool separate(const void* a,size_t as,const void* b,size_t bs){
 const auto x=reinterpret_cast<uintptr_t>(a),y=reinterpret_cast<uintptr_t>(b);
 return x<=y ? as<=y-x : bs<=x-y;
}
}
bool Registry::scene_ready_main_device_available(const RecordRef &ref, Object *object) {
 if(ref.registry!=this)return false;
 auto* rd=Object::cast_to<RenderingDevice>(object);
 if(!rd || !rd->is_main_instance)return false;
 MutexLock lock(mutex);
 const auto& record=records[ref.slot];
 if(record.owner_thread!=Thread::get_caller_id() || record.quiescing || record.close_started || record.scene_ready_state!=Record::SceneReadyState::PENDING)return false;
 for(const auto& device:device_records)if(device.object==object && device.epoch && device.live)return true;
 return false;
}
Error Registry::device_created(Object* object) {
 MutexLock lock(mutex);
 if(phase!=Phase::RUNNING)return ERR_UNAUTHORIZED;
 if(!object || device_epochs.exhausted)return ERR_PARAMETER_RANGE_ERROR;
 for(auto& d:device_records)if(!d.epoch){
  uint64_t epoch;if(!device_epochs.take(epoch))return ERR_PARAMETER_RANGE_ERROR;
  d.object=object;d.epoch=epoch;d.thread=Thread::get_caller_id();d.execution_thread=d.thread;d.live=true;return OK;
 }
 return ERR_PARAMETER_RANGE_ERROR;
}
Error Registry::device_execution_moved(Object *object, uint64_t previous, uint64_t next) {
 MutexLock lock(mutex);
 for (auto &device : device_records) {
  if (device.object != object || !object) continue;
  if (!device.live || !device.epoch || device.execution_thread != previous || next != Thread::get_caller_id()) return ERR_UNAUTHORIZED;
  if (previous == next) return OK;
  if (device.execution_migrated) return ERR_ALREADY_IN_USE;
  device.execution_thread = next;
  device.execution_migrated = true;
  return OK;
 }
 return ERR_DOES_NOT_EXIST;
}
void Registry::device_closed(Object* object) {
 MutexLock lock(mutex);
 for(auto& d:device_records)if(d.object==object){d.live=false;return;}
}
void Registry::device_lost(Object* object) { device_closed(object); }
void Registry::device_loss_identity(Object* object,uint64_t& registry,uint64_t& epoch) {
 MutexLock lock(mutex);registry=0;epoch=0;
 for(const auto& d:device_records)if(d.object==object && object){registry=instance;epoch=d.epoch;return;}
}
void Registry::device_lost(uint64_t registry,uint64_t epoch) {
 MutexLock lock(mutex);if(registry!=instance || !epoch)return;
 for(auto& d:device_records)if(d.epoch==epoch){d.live=false;return;}
}
void Registry::reclaim_device_locked(uint32_t slot) {
 auto& d=device_records[slot];
 if(d.object==nullptr && d.refs==0)d=DeviceRecord{};
}
void Registry::device_destroyed(Object* object) {
 MutexLock lock(mutex);
 for(uint32_t i=0;i<RECORD_CAPACITY;++i)if(device_records[i].object==object && object){
  auto& d=device_records[i];d.live=false;d.object=nullptr;
  reclaim_device_locked(i);try_finalize();return;
 }
}
Error Registry::device_ref_locked(uint64_t registry,uint64_t serial,bool session,DeviceRef*& out){
 if(registry!=instance || !serial)return ERR_INVALID_DATA;
 for(auto& ref:device_refs)if(ref.serial==serial){if(ref.session!=session)return ERR_INVALID_DATA;out=&ref;return OK;}
 return ERR_DOES_NOT_EXIST;
}
Error Registry::issue_device_ref_locked(uint32_t device,uint32_t library,bool session,uint64_t& out){
 auto& record=records[library];
 if(record.private_refs>=private_limit || device_ref_serials.exhausted)return ERR_PARAMETER_RANGE_ERROR;
 for(auto& ref:device_refs)if(!ref.serial){
  if(!device_ref_serials.take(out))return ERR_PARAMETER_RANGE_ERROR;
  ref={out,device,library,session};++record.private_refs;++device_records[device].refs;return OK;
 }
 return ERR_PARAMETER_RANGE_ERROR;
}
GDExtensionDeviceSessionResultV1 Registry::bind_device_session(const GDExtensionDeviceExitAbi* abi,InitializationLevelContextRef frame,Object* object,GDExtensionDeviceRegistryContextRef* context,GDExtensionOriginalDeviceSessionRef* session){
 return bind_device_session_in_frame(abi,frame.registry_instance,frame.handle_serial,ViewDomain::INITIALIZE,object,context,session);
}
GDExtensionDeviceSessionResultV1 Registry::bind_scene_ready_device_session(const GDExtensionDeviceExitAbi* abi,GDExtensionDeviceExitSceneReadyContextRef frame,Object* object,GDExtensionDeviceRegistryContextRef* context,GDExtensionOriginalDeviceSessionRef* session){
 return bind_device_session_in_frame(abi,frame.registry_instance,frame.handle_serial,ViewDomain::SCENE_READY,object,context,session);
}
GDExtensionDeviceSessionResultV1 Registry::bind_device_session_in_frame(const GDExtensionDeviceExitAbi* abi,uint64_t frame_instance,uint64_t frame_serial,ViewDomain domain,Object* object,GDExtensionDeviceRegistryContextRef* context,GDExtensionOriginalDeviceSessionRef* session){
 if(aligned_pointer(abi) && abi->abi_version!=1)return result(ERR_INVALID_PARAMETER,1,1,abi->abi_version);
 if(!aligned_pointer(abi) || abi->abi_version!=1 || abi->struct_size!=sizeof(*abi) || abi->reserved || !aligned_pointer(context) || !aligned_pointer(session) || !separate(context,sizeof(*context),session,sizeof(*session)) || !separate(abi,sizeof(*abi),context,sizeof(*context)) || !separate(abi,sizeof(*abi),session,sizeof(*session)) || context->handle_serial || context->registry_instance || session->handle_serial || session->registry_instance)return result(ERR_INVALID_PARAMETER,2);
 MutexLock lock(mutex);View* view=nullptr;
 if(!frame_instance || !frame_serial)return result(ERR_INVALID_PARAMETER,4);
 if(frame_instance!=instance)return result(ERR_INVALID_DATA,5);
 Error error=resolve_borrow_locked(frame_instance,frame_serial,domain,view);if(error!=OK)return result(error,10);
 auto& record=records[view->record];
 if(domain==ViewDomain::SCENE_READY && (RegistrationContext::current->level_phase!=LevelPhase::SCENE_READY || record.scene_ready_state!=Record::SceneReadyState::IN_FRAME))return result(ERR_UNAUTHORIZED,10);
 if(record.guard_enrollment!=GuardEnrollment::QUALIFIED || record.quiescing || record.close_started || record.owner_thread!=Thread::get_caller_id())return result(ERR_UNAUTHORIZED,10);
 if(domain==ViewDomain::SCENE_READY && record.scene_ready_flavor==Record::SceneReadyFlavor::V2 &&
   (!(record.scene_install_plan.selected_features&GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2) || !scene_install_plan_matches_locked(record)))return result(ERR_UNAVAILABLE,GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1);

 uint32_t index=INVALID_SLOT;
 for(uint32_t i=0;i<RECORD_CAPACITY;++i)if(device_records[i].object==object && object){index=i;break;}
 if(index==INVALID_SLOT)return result(ERR_DOES_NOT_EXIST,7);
 auto& d=device_records[index];if(!d.live || d.thread!=Thread::get_caller_id())return result(ERR_UNAUTHORIZED,10);
 // One original host association for each RD; attachments reuse it, including
 // after library reload. No extension-supplied association or session number.
 uint32_t free=0;for(const auto& ref:device_refs)free+=!ref.serial;
 if(free<2)return result(ERR_PARAMETER_RANGE_ERROR,11,VIEW_CAPACITY,uint64_t(VIEW_CAPACITY-free)+2);
 if(private_limit<2 || record.private_refs>private_limit-2)return result(ERR_PARAMETER_RANGE_ERROR,11,private_limit,uint64_t(record.private_refs)+2);
 if(device_ref_serials.exhausted || device_ref_serials.next==UINT64_MAX)return result(ERR_PARAMETER_RANGE_ERROR,11,device_ref_serials.exhausted?0:1,2);
 if(!d.session && d.sessions.exhausted)return result(ERR_PARAMETER_RANGE_ERROR,12);
 uint64_t c=0,s=0;
 error=issue_device_ref_locked(index,view->record,false,c);if(error!=OK)return result(error,10);
 error=issue_device_ref_locked(index,view->record,true,s);CRASH_COND(error!=OK);
 if(!d.session){const bool issued=d.sessions.take(d.session);CRASH_COND(!issued);}
 *context={instance,c};*session={instance,s};return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::bind_main_scene_ready_device_session(const GDExtensionDeviceExitAbi *abi, GDExtensionDeviceExitSceneReadyContextRef frame, GDExtensionDeviceRegistryContextRef *context, GDExtensionOriginalDeviceSessionRef *session, uint64_t *object_id) {
 if (!aligned_pointer(abi) || !aligned_pointer(context) || !aligned_pointer(session) || !aligned_pointer(object_id)) return result(ERR_INVALID_PARAMETER, 2);
 if (abi->abi_version != 1) return result(ERR_INVALID_PARAMETER, 1, 1, abi->abi_version);
 if (abi->struct_size != sizeof(*abi) || abi->reserved || context->registry_instance || context->handle_serial || session->registry_instance || session->handle_serial || *object_id ||
     !separate(abi,sizeof(*abi),context,sizeof(*context)) || !separate(abi,sizeof(*abi),session,sizeof(*session)) || !separate(abi,sizeof(*abi),object_id,sizeof(*object_id)) ||
     !separate(context,sizeof(*context),session,sizeof(*session)) || !separate(context,sizeof(*context),object_id,sizeof(*object_id)) || !separate(session,sizeof(*session),object_id,sizeof(*object_id))) return result(ERR_INVALID_PARAMETER,2);
 MutexLock lock(mutex);
 View *view = nullptr;
 Error error = resolve_borrow_locked(frame.registry_instance, frame.handle_serial, ViewDomain::SCENE_READY, view);
 if (error != OK) return result(error,14);
 auto &record = records[view->record];
 if (!RegistrationContext::current || RegistrationContext::current->level_phase != LevelPhase::SCENE_READY || record.scene_ready_state != Record::SceneReadyState::IN_FRAME) return result(ERR_UNAUTHORIZED,14);
 if (record.owner_thread != Thread::get_caller_id()) return result(ERR_UNAUTHORIZED,15);
 if (record.guard_enrollment != GuardEnrollment::QUALIFIED || record.quiescing || record.close_started || record.qualification_main_issued) return result(ERR_UNAUTHORIZED,16);
 if(record.scene_ready_flavor==Record::SceneReadyFlavor::V2 &&
   (!(record.scene_install_plan.selected_features&GDEXTENSION_SCENE_INSTALL_RD_PAIR_V2) || !scene_install_plan_matches_locked(record)))return result(ERR_UNAVAILABLE,GDEXTENSION_DEVICE_SESSION_REASON_NATIVE_UNSUPPORTED_V1);

 auto *server = RenderingServer::get_singleton();
 auto *rd = server ? server->get_rendering_device() : nullptr;
 if (!rd || rd != RenderingDevice::get_singleton() || !rd->is_main_instance || ObjectDB::get_instance(rd->get_instance_id()) != rd) return result(ERR_UNAVAILABLE,18);
 uint32_t index = INVALID_SLOT;
 for (uint32_t i=0; i<RECORD_CAPACITY; ++i) if (device_records[i].object==rd && device_records[i].live && device_records[i].epoch) { index=i; break; }
 if (index==INVALID_SLOT) return result(ERR_UNAVAILABLE,18);
 auto &device = device_records[index];
 if (!device.execution_thread || device.execution_thread != rd->render_thread_id) return result(ERR_UNAUTHORIZED,15);
 uint32_t free=0; for (const auto &ref : device_refs) free += !ref.serial;
 if (free<2 || private_limit<2 || record.private_refs>private_limit-2 || device_ref_serials.exhausted || device_ref_serials.next==UINT64_MAX || (!device.session && device.sessions.exhausted)) return result(ERR_PARAMETER_RANGE_ERROR,19);
 uint64_t c=0,s=0;
 error=issue_device_ref_locked(index,view->record,false,c); CRASH_COND(error!=OK);
 error=issue_device_ref_locked(index,view->record,true,s); CRASH_COND(error!=OK);
 for (auto &ref : device_refs) if (ref.serial==c || ref.serial==s) ref.qualification_main=true;
 if (!device.session) { const bool issued=device.sessions.take(device.session); CRASH_COND(!issued); }
 record.qualification_main_issued=true;
 *context={instance,c}; *session={instance,s}; *object_id=rd->get_instance_id();
 return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::copy_device_session(GDExtensionDeviceRegistryContextRef context,GDExtensionOriginalDeviceSessionRef session,bool exporting,GDExtensionDeviceSessionViewV1* out){
 if(!aligned_pointer(out))return result(ERR_INVALID_PARAMETER,2);
 MutexLock lock(mutex);DeviceRef *c=nullptr,*s=nullptr;
 Error error=device_ref_locked(context.registry_instance,context.handle_serial,false,c);if(error!=OK)return reference_error(error,context.registry_instance,context.handle_serial,instance);
 error=device_ref_locked(session.registry_instance,session.handle_serial,true,s);if(error!=OK)return reference_error(error,session.registry_instance,session.handle_serial,instance);
 if(c->device!=s->device || c->library!=s->library)return result(ERR_INVALID_DATA,9);
 const auto& d=device_records[c->device];const auto& r=records[c->library];
 if(r.owner_thread!=Thread::get_caller_id())return result(ERR_UNAUTHORIZED,10);
 const bool allowed=d.live && !r.quiescing && !r.close_started && r.guard_enrollment==GuardEnrollment::QUALIFIED;
 if(exporting && !allowed)return result(ERR_UNAUTHORIZED,10);
 *out={d.epoch,d.session,1,uint32_t(d.live?1:(d.object?2:3)),uint32_t(allowed)};return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::resolve_device_session(GDExtensionDeviceRegistryContextRef context,uint64_t epoch,uint64_t session,GDExtensionOriginalDeviceSessionRef* out){
 if(!aligned_pointer(out) || out->registry_instance || out->handle_serial)return result(ERR_INVALID_PARAMETER,2);
 if(!epoch || !session)return result(ERR_INVALID_PARAMETER,3);
 MutexLock lock(mutex);DeviceRef* c=nullptr;Error error=device_ref_locked(context.registry_instance,context.handle_serial,false,c);if(error!=OK)return reference_error(error,context.registry_instance,context.handle_serial,instance);
 const auto& d=device_records[c->device];if(d.epoch!=epoch)return result(ERR_INVALID_DATA,6,d.epoch,epoch);
 if(d.session!=session)return result(ERR_DOES_NOT_EXIST,7);
 if(records[c->library].owner_thread!=Thread::get_caller_id())return result(ERR_UNAUTHORIZED,10);
 uint32_t used=0;for(const auto& ref:device_refs)used+=ref.serial!=0;
 if(used==VIEW_CAPACITY)return result(ERR_PARAMETER_RANGE_ERROR,11,VIEW_CAPACITY,uint64_t(used)+1);
 if(records[c->library].private_refs>=private_limit)return result(ERR_PARAMETER_RANGE_ERROR,11,private_limit,uint64_t(records[c->library].private_refs)+1);
 if(device_ref_serials.exhausted)return result(ERR_PARAMETER_RANGE_ERROR,11,0,1);
 uint64_t serial;error=issue_device_ref_locked(c->device,c->library,true,serial);if(error==OK)*out={instance,serial};return result(error,error==OK?0:11,private_limit,records[c->library].private_refs+1);
}
GDExtensionDeviceSessionResultV1 Registry::release_device_reference(uint64_t registry,uint64_t serial,bool session){
 MutexLock lock(mutex);DeviceRef* ref=nullptr;Error error=device_ref_locked(registry,serial,session,ref);if(error!=OK)return reference_error(error,registry,serial,instance);
 auto slot=ref->library;auto device=ref->device;*ref={};CRASH_COND(!records[slot].private_refs || !device_records[device].refs);--records[slot].private_refs;--device_records[device].refs;reclaim_device_locked(device);reclaim(slot);try_finalize();return result(OK);
}
static GDExtensionDeviceSessionResultV1 bind_session(const GDExtensionDeviceExitAbi* abi,GDExtensionDeviceExitInitializationLevelContextRef frame,GDExtensionObjectPtr rd,GDExtensionDeviceRegistryContextRef* c,GDExtensionOriginalDeviceSessionRef* s){return Registry::get_singleton().bind_device_session(abi,{frame.registry_instance,frame.handle_serial},static_cast<Object*>(rd),c,s);}
static GDExtensionDeviceSessionResultV1 bind_ready_session(const GDExtensionDeviceExitAbi* abi,GDExtensionDeviceExitSceneReadyContextRef frame,GDExtensionObjectPtr rd,GDExtensionDeviceRegistryContextRef* c,GDExtensionOriginalDeviceSessionRef* s){return Registry::get_singleton().bind_scene_ready_device_session(abi,frame,static_cast<Object*>(rd),c,s);}
static GDExtensionDeviceSessionResultV1 copy_session(GDExtensionDeviceRegistryContextRef c,GDExtensionOriginalDeviceSessionRef s,uint32_t exporting,GDExtensionDeviceSessionViewV1* out){if(exporting>1)return result(ERR_INVALID_PARAMETER,2);return Registry::get_singleton().copy_device_session(c,s,exporting,out);}
static GDExtensionDeviceSessionResultV1 resolve_session(GDExtensionDeviceRegistryContextRef c,uint64_t epoch,uint64_t session,GDExtensionOriginalDeviceSessionRef* out){return Registry::get_singleton().resolve_device_session(c,epoch,session,out);}
static GDExtensionDeviceSessionResultV1 release_context(GDExtensionDeviceRegistryContextRef c){return Registry::get_singleton().release_device_reference(c.registry_instance,c.handle_serial,false);}
static GDExtensionDeviceSessionResultV1 release_session(GDExtensionOriginalDeviceSessionRef s){return Registry::get_singleton().release_device_reference(s.registry_instance,s.handle_serial,true);}
static GDExtensionDeviceSessionResultV1 bind_main_ready_session(const GDExtensionDeviceExitAbi *abi,GDExtensionDeviceExitSceneReadyContextRef frame,GDExtensionDeviceRegistryContextRef *c,GDExtensionOriginalDeviceSessionRef *s,uint64_t *id) { return Registry::get_singleton().bind_main_scene_ready_device_session(abi,frame,c,s,id); }
void register_qualification_interfaces();
void register_device_session_interfaces(){
 register_qualification_interfaces();
 GDExtension::register_interface_function("device_session_bind_main_scene_ready_v1",(GDExtensionInterfaceFunctionPtr)&bind_main_ready_session);
 GDExtension::register_interface_function("device_session_bind_v1",(GDExtensionInterfaceFunctionPtr)&bind_session);
 GDExtension::register_interface_function("device_session_bind_scene_ready_v1",(GDExtensionInterfaceFunctionPtr)&bind_ready_session);
 GDExtension::register_interface_function("device_session_copy_v1",(GDExtensionInterfaceFunctionPtr)&copy_session);
 GDExtension::register_interface_function("device_session_resolve_v1",(GDExtensionInterfaceFunctionPtr)&resolve_session);
 GDExtension::register_interface_function("device_session_release_context_v1",(GDExtensionInterfaceFunctionPtr)&release_context);
 GDExtension::register_interface_function("device_session_release_session_v1",(GDExtensionInterfaceFunctionPtr)&release_session);
}
}
