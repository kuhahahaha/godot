#include "core/extension/gdextension_interface.gen.h"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>
#include <cstring>
#include <atomic>
#include <CommonCrypto/CommonDigest.h>
#include "core/error/error_list.h"

#ifndef PROBE_TAG
#define PROBE_TAG "one"
#endif
#ifndef PROBE_ERROR
#define PROBE_ERROR 0
#endif
#ifndef PROBE_SKIP_READY
#define PROBE_SKIP_READY 0
#endif

namespace {
struct State { uint32_t calls = 0; } state;
GDExtensionClassLibraryPtr library = nullptr;
GDExtensionDeviceExitGuardToken guard{};
const GDExtensionDeviceExitAbi abi{1, sizeof(GDExtensionDeviceExitAbi), 0};
GDExtensionInterfaceDeviceExitRegisterGuardV1 enroll = nullptr;
GDExtensionInterfaceDeviceExitBorrowInitializeLevelV1 borrow_init = nullptr;
GDExtensionInterfaceDeviceExitAfterBindingInitializeV1 after_init = nullptr;
GDExtensionInterfaceDeviceExitBorrowDeinitLevelV1 borrow_deinit = nullptr;
GDExtensionInterfaceDeviceExitBeginUserTerminatorV1 begin_user = nullptr;
GDExtensionInterfaceDeviceExitAfterUserTerminatorV1 after_user = nullptr;
GDExtensionInterfaceDeviceExitAfterBindingDeinitV1 after_binding = nullptr;
GDExtensionInterfaceDeviceExitPublishExitResultV1 publish = nullptr;
GDExtensionInterfaceDeviceExitRegisterSceneReadyV1 install_ready = nullptr;
GDExtensionInterfaceDeviceExitSceneReadyScheduleObjectCleanupV1 schedule_ready = nullptr;
GDExtensionInterfaceDeviceSessionBindSceneReadyV1 bind_ready = nullptr;
GDExtensionInterfaceDeviceExitSceneReadyAbortV1 abort_ready = nullptr;

GDExtensionInterfaceDeviceExitBorrowGuardedLibraryV1 borrow_library;
GDExtensionInterfaceDeviceExitBorrowLevelCleanupV1 borrow_cleanup;
GDExtensionInterfaceDeviceExitAcquireCodeHoldV1 acquire_hold;
GDExtensionInterfaceDeviceExitReleaseCodeHoldV1 release_hold;
GDExtensionInterfaceDeviceSessionBindMainSceneReadyV1 bind_main;
GDExtensionInterfaceDeviceSessionQualificationOpenV1 open_bridge;
GDExtensionInterfaceDeviceSessionQualificationCloseV1 close_bridge;
GDExtensionInterfaceDeviceSessionReleaseContextV1 release_context;
GDExtensionInterfaceDeviceSessionReleaseSessionV1 release_session;
GDExtensionDeviceExitLibraryCodeHoldRef code_hold{};
GDExtensionInterfaceObjectGetInstanceFromId object_from_id;
GDExtensionDeviceExitSceneReadyContextRef expired_ready{};

GDExtensionInterfaceDeviceSessionQualificationStorageCreateV1 storage_create;
GDExtensionInterfaceDeviceSessionQualificationStorageRetireV1 storage_retire;
GDExtensionInterfaceDeviceSessionQualificationStorageRetireObserveV1 storage_observe;
GDExtensionInterfaceDeviceSessionQualificationStorageRetireReleaseV1 storage_release;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitCreateV1 unit_create;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireV1 unit_retire;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireObserveV1 unit_observe;
GDExtensionInterfaceDeviceSessionQualificationNativeUnitRetireReleaseV1 unit_release;
GDExtensionInterfaceDeviceExitBootstrapLibraryV1 bootstrap_library;
GDExtensionInterfaceDeviceExitRetainLibraryViewV1 retain_library_view;
GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1 copy_library_snapshot;
GDExtensionInterfaceDeviceExitReleaseLibraryViewV1 release_library_view;
uint64_t library_generation=0;
void require(int32_t error, const char *where) {
 if (!error) { return; }
 std::fprintf(stderr, "PHS1_FIXTURE_FATAL tag=%s where=%s error=%d\n", PROBE_TAG, where, error);
 std::_Exit(91);
}

GDExtensionInterfaceDeviceSessionQualificationRecordV1 record_ticket;
GDExtensionInterfaceDeviceSessionQualificationObserveV1 observe_ticket;
GDExtensionInterfaceDeviceSessionQualificationFactsV1 facts_ticket;
GDExtensionInterfaceDeviceSessionQualificationCopyV1 copy_ticket;
GDExtensionInterfaceDeviceSessionQualificationReleaseV1 release_ticket;
GDExtensionInterfaceStringNameNewWithUtf8Chars name_new;
GDExtensionInterfaceVariantGetPtrDestructor type_destroy;
GDExtensionInterfaceVariantGetPtrConstructor type_construct;
GDExtensionInterfaceClassdbConstructObject2 construct_object;
GDExtensionInterfaceClassdbGetMethodBind get_method;
GDExtensionInterfaceObjectMethodBindPtrcall call_method;
GDExtensionInterfaceGetVariantFromTypeConstructor from_type;
GDExtensionInterfaceVariantNewNil variant_nil;
GDExtensionInterfaceVariantDestroy variant_destroy;
GDExtensionInterfaceArraySetTyped set_array_type;
GDExtensionInterfaceVariantGetPtrBuiltinMethod get_builtin;
GDExtensionInterfaceGlobalGetSingleton global_singleton;
GDExtensionInterfaceCallableCustomCreate2 callable_create;
struct alignas(8) Opaque {unsigned char bytes[64]={};};
void method(const char *klass,const char *name,uint64_t hash,void *object,const GDExtensionConstTypePtr *args,void *out=nullptr) {
 Opaque k,n;name_new(&k,klass);name_new(&n,name);auto m=get_method(&k,&n,hash);if(!m)std::_Exit(130);call_method(m,object,args,out);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&k);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&n);
}
uint64_t set_create(void *rd,uint64_t shader,uint64_t buffer) {
 Opaque klass;name_new(&klass,"RDUniform");void *uniform=construct_object(&klass);if(!uniform)std::_Exit(131);
 int64_t kind=8,binding=0,index=0;const void *a[]={&kind};method("RDUniform","set_uniform_type",1664894931,uniform,a);a[0]=&binding;method("RDUniform","set_binding",1286410249,uniform,a);a[0]=&buffer;method("RDUniform","add_id",2722037293,uniform,a);
 Opaque array,nil,object_variant,append;type_construct(GDEXTENSION_VARIANT_TYPE_ARRAY,0)(&array,nullptr);variant_nil(&nil);set_array_type(&array,GDEXTENSION_VARIANT_TYPE_OBJECT,&klass,&nil);
 from_type(GDEXTENSION_VARIANT_TYPE_OBJECT)(&object_variant,&uniform);name_new(&append,"append");auto fn=get_builtin(GDEXTENSION_VARIANT_TYPE_ARRAY,&append,3316032543);if(!fn)std::_Exit(132);a[0]=&object_variant;fn(&array,a,nullptr,1);
 uint64_t set=0;const void *args[]={&array,&shader,&index};method("RenderingDevice","uniform_set_create",2280795797,rd,args,&set);
 variant_destroy(&object_variant);variant_destroy(&nil);type_destroy(GDEXTENSION_VARIANT_TYPE_ARRAY)(&array);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&klass);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&append);if(!set)std::_Exit(133);return set;
}
// Test-only cross-thread join. CodeHold stays live throughout SceneReady;
// no stack data is released until the engine destroys its queued Callable.
struct SetTask { void *rd; uint64_t shader,buffer,result=0; std::atomic<bool> released{false}; };
void set_call(void *userdata,const GDExtensionConstVariantPtr *,GDExtensionInt argc,GDExtensionVariantPtr result,GDExtensionCallError *error) {
 auto &task=*static_cast<SetTask *>(userdata);
 if(argc!=0)std::_Exit(181);
 task.result=set_create(task.rd,task.shader,task.buffer);
 variant_nil(result);error->error=GDEXTENSION_CALL_OK;
 std::fprintf(stderr,"TEST_RENDER_SET_CREATED rid=%llu\n",(unsigned long long)task.result);
}
void set_free(void *userdata) { static_cast<SetTask *>(userdata)->released.store(true,std::memory_order_release); }
uint64_t set_create_on_render_thread(void *rd,uint64_t shader,uint64_t buffer) {
 SetTask task{rd,shader,buffer};Opaque server_name,callback;name_new(&server_name,"RenderingServer");
 void *server=global_singleton(&server_name);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&server_name);if(!server)std::_Exit(182);
 GDExtensionCallableCustomInfo2 info{};info.callable_userdata=&task;info.token=library;info.call_func=&set_call;info.free_func=&set_free;
 callable_create(&callback,&info);const void *args[]={&callback};method("RenderingServer","call_on_render_thread",1611583062,server,args);
 type_destroy(GDEXTENSION_VARIANT_TYPE_CALLABLE)(&callback);
 for(unsigned i=0;i<5000 && !task.released.load(std::memory_order_acquire);++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 if(!task.released.load(std::memory_order_acquire) || !task.result)std::_Exit(183);
 return task.result;
}
GDExtensionInterfaceDeviceSessionQualificationRtConfigureV1 rt_configure;
GDExtensionInterfaceDeviceSessionQualificationRtReserveV1 rt_reserve;
GDExtensionInterfaceDeviceSessionQualificationRtActivateV1 rt_activate;
GDExtensionInterfaceDeviceSessionQualificationRtObserveV1 rt_observe;
GDExtensionInterfaceDeviceSessionQualificationRtReleaseV1 rt_release;
uint64_t rt_operation=1;bool rt_configured=false,rt_abort_tested=false;
GDExtensionDeviceSessionResultV1 invoke_rt(GDExtensionQualificationBridgeRefV1 bridge,uint32_t kind,const void *request,uint32_t request_size,const void *payload,uint64_t payload_size,void *out,uint32_t out_size,GDExtensionQualificationUploadSegmentV1 *segments=nullptr,uint32_t segment_capacity=0) {
 memset(out,0,out_size);if(segment_capacity)memset(segments,0,segment_capacity*sizeof(*segments));
 if(!rt_configured){GDExtensionQualificationRtConfigV1 config{{1,sizeof(config),0},1,312,200,2,65536};require(rt_configure(bridge,&config).native_error,"rt_configure");rt_configured=true;}
 GDExtensionQualificationRtRequestV1 header{{1,sizeof(header),0},kind,0,rt_operation++,payload_size,request_size,out_size,segment_capacity,0};
 GDExtensionQualificationRtRefV1 ref{},again{};auto accepted=rt_reserve(bridge,&header,request,payload,&ref);if(accepted.native_error)return accepted;
 if(!rt_abort_tested){require(rt_release(bridge,ref).native_error,"rt_abort_before_activate");if(!rt_activate(bridge,ref).native_error)std::_Exit(171);header.operation_serial=rt_operation++;ref={};require(rt_reserve(bridge,&header,request,payload,&ref).native_error,"rt_reserve_after_abort");rt_abort_tested=true;}
 require(rt_reserve(bridge,&header,request,payload,&again).native_error,"rt_exact_reserve_retry");if(memcmp(&ref,&again,sizeof(ref)))std::_Exit(172);
 require(rt_activate(bridge,ref).native_error,"rt_activate");if(!rt_activate(bridge,ref).native_error)std::_Exit(173);
 GDExtensionQualificationRtObservationV1 observation{};
 for(unsigned pass=0;pass<5000;++pass){require(rt_observe(bridge,ref,&observation,out,out_size,segments,segment_capacity).native_error,"rt_observe");if(observation.state!=1)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 if(observation.state!=2 || observation.kind!=kind || observation.operation_serial!=header.operation_serial || observation.slot_generation!=ref.handle_serial || observation.result_bytes!=out_size || observation.abi.struct_size!=sizeof(observation))std::_Exit(174);
 again={};require(rt_reserve(bridge,&header,request,payload,&again).native_error,"rt_returned_reserve_retry");if(memcmp(&ref,&again,sizeof(ref)))std::_Exit(175);
 require(rt_release(bridge,ref).native_error,"rt_owner_ack");if(!rt_release(bridge,ref).native_error)std::_Exit(176);
 std::fprintf(stderr,"QRT_NATIVE_RETURN kind=%u operation=%llu slot=%llu result=%u segments=%u error=%d reply_ack=1\n",kind,(unsigned long long)header.operation_serial,(unsigned long long)ref.handle_serial,observation.result_bytes,observation.segment_count,observation.call_result.native_error);
 return observation.call_result;
}
GDExtensionDeviceSessionResultV1 wrapped_storage_create(GDExtensionQualificationBridgeRefV1 b,const GDExtensionStorageCreateRequestV1 *q,GDExtensionStorageBackingV1 *o){auto r=invoke_rt(b,2,q,sizeof(*q),nullptr,0,o,sizeof(*o));if(!r.native_error){GDExtensionQualificationBackingV1 backing{};auto observed=invoke_rt(b,1,&o->rid,8,nullptr,0,&backing,sizeof(backing));require(observed.native_error,"rt_backing");if(backing.rid!=o->rid || backing.backing_serial!=o->backing_serial)std::_Exit(177);}return r;}
GDExtensionDeviceSessionResultV1 wrapped_storage_retire(GDExtensionQualificationBridgeRefV1 b,const GDExtensionStorageRetireRequestV1 *q,GDExtensionStorageRetireRefV1 *r,GDExtensionStorageRetireReportV1 *o){struct Pair{GDExtensionStorageRetireRefV1 ref;GDExtensionStorageRetireReportV1 report;}pair{};static_assert(sizeof(pair)==136);auto result=invoke_rt(b,3,q,sizeof(*q),nullptr,0,&pair,sizeof(pair));*r=pair.ref;*o=pair.report;return result;}
GDExtensionDeviceSessionResultV1 wrapped_unit_create(GDExtensionQualificationBridgeRefV1 b,const GDExtensionNativeUnitCreateRequestV1 *q,const void *p,uint64_t n,GDExtensionNativeUnitBackingV1 *o){return invoke_rt(b,4,q,sizeof(*q),p,n,o,sizeof(*o));}
GDExtensionDeviceSessionResultV1 wrapped_unit_retire(GDExtensionQualificationBridgeRefV1 b,const GDExtensionNativeUnitRetireRequestV1 *q,GDExtensionNativeUnitRetireRefV1 *r,GDExtensionNativeUnitRetireReportV1 *o){struct Pair{GDExtensionNativeUnitRetireRefV1 ref;GDExtensionNativeUnitRetireReportV1 report;}pair{};static_assert(sizeof(pair)==184);auto result=invoke_rt(b,5,q,sizeof(*q),nullptr,0,&pair,sizeof(pair));*r=pair.ref;*o=pair.report;return result;}

GDExtensionDeviceSessionResultV1 wrapped_record(GDExtensionQualificationBridgeRefV1 b,const GDExtensionQualificationRecordV1 *q,GDExtensionQualificationTicketRefV1 *t,GDExtensionQualificationReportV1 *o){struct Pair{GDExtensionQualificationTicketRefV1 ref;GDExtensionQualificationReportV1 report;}pair{};static_assert(sizeof(pair)==200);auto result=invoke_rt(b,6,q,sizeof(*q),nullptr,0,&pair,sizeof(pair));*t=pair.ref;*o=pair.report;return result;}
GDExtensionDeviceSessionResultV1 wrapped_prepare(GDExtensionQualificationBridgeRefV1 b,const GDExtensionQualificationUploadPrepareV1 *q,const GDExtensionQualificationUploadPlanV1 *p,uint32_t n,GDExtensionQualificationTicketRefV1 *t,GDExtensionQualificationReportV1 *o){struct Pair{GDExtensionQualificationTicketRefV1 ref;GDExtensionQualificationReportV1 report;}pair{};auto result=invoke_rt(b,7,q,sizeof(*q),p,uint64_t(n)*sizeof(*p),&pair,sizeof(pair));*t=pair.ref;*o=pair.report;return result;}
GDExtensionDeviceSessionResultV1 wrapped_upload(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,const GDExtensionQualificationUploadRequestV1 *q,const void *p,uint32_t n,GDExtensionQualificationUploadSegmentV1 *s,uint32_t c,GDExtensionQualificationUploadReportV1 *o){std::vector<uint8_t> payload(16+size_t(n));memcpy(payload.data(),&t,16);memcpy(payload.data()+16,p,n);return invoke_rt(b,8,q,sizeof(*q),payload.data(),payload.size(),o,sizeof(*o),s,c);}
GDExtensionDeviceSessionResultV1 wrapped_prepared(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,const GDExtensionQualificationRecordV1 *q,GDExtensionQualificationReportV1 *o){return invoke_rt(b,9,q,sizeof(*q),&t,sizeof(t),o,sizeof(*o));}
int32_t ready(void *userdata,GDExtensionDeviceExitSceneReadyContextRef context) {
 if(userdata!=&state)return ERR_INVALID_DATA;state.calls++;expired_ready=context;
 GDExtensionDeviceRegistryContextRef c{};GDExtensionOriginalDeviceSessionRef session{};uint64_t id=0;
 require(bind_main(&abi,context,&c,&session,&id).native_error,"bind_main");
 GDExtensionQualificationBridgeRefV1 bridge{};require(open_bridge(&abi,context,c,session,code_hold,&bridge).native_error,"open_bridge");
 GDExtensionStorageCreateRequestV1 storage_request{abi,64,3,0,41,1,0};GDExtensionStorageBackingV1 backing{};require(storage_create(bridge,&storage_request,&backing).native_error,"storage_create");
 GDExtensionNativeUnitBackingV1 units[2]{};const char *paths[]={std::getenv("QB1_CAPTURE_CONTAINER"),std::getenv("QB1_BODY_CONTAINER")};
 for(unsigned i=0;i<2;++i) {
  if(!paths[i])std::_Exit(134);std::ifstream input(paths[i],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});if(bytes.empty())std::_Exit(135);
  GDExtensionNativeUnitCreateRequestV1 request{};request.abi=abi;request.library_generation=library_generation;request.preparation_generation=21;request.operation_serial=i+1;request.container_byte_count=bytes.size();request.record_slot=2;request.physical_unit=i;CC_SHA256(bytes.data(),bytes.size(),request.container_digest);
  require(unit_create(bridge,&request,bytes.data(),bytes.size(),&units[i]).native_error,"unit_create");if(units[i].state!=1 || units[i].known_mask!=3 || units[i].possible_mask)std::_Exit(136);
 }
 void *rd=object_from_id(id);uint64_t sets[]={set_create_on_render_thread(rd,units[0].shader_rid,backing.rid),set_create_on_render_thread(rd,units[1].shader_rid,backing.rid)};
 GDExtensionQualificationRecordV1 request{};request.abi=abi;request.record_generation=19;request.request_nonce=23;request.library_generation=library_generation;request.qualification_session_serial=7;request.range_end=4;request.attempt=1;request.sample_count=4;request.body_groups_x=1;request.capture_pipeline=units[0].pipeline_rid;request.capture_set=sets[0];request.body_pipeline=units[1].pipeline_rid;request.body_set=sets[1];request.readback_buffer=backing.rid;request.source_allocation_generation=41;request.observed_backing_serial=backing.backing_serial;request.readback_size=16;
 const uint32_t capture_push[]={3,4,0,0},body_push[]={0,4,0,0};memcpy(request.capture_push,capture_push,16);memcpy(request.body_push,body_push,16);
 GDExtensionQualificationTicketRefV1 ticket{},repeat{};GDExtensionQualificationReportV1 report{},again{};
 auto invalid=request;invalid.body_groups_x=2;if(record_ticket(bridge,&invalid,&ticket,&report).native_error==OK || ticket.handle_serial)std::_Exit(137);
 require(record_ticket(bridge,&request,&ticket,&report).native_error,"record");if(!ticket.handle_serial || report.state==0)std::_Exit(138);
 require(record_ticket(bridge,&request,&repeat,&again).native_error,"record_repeat");if(memcmp(&ticket,&repeat,sizeof(ticket)) || memcmp(&report,&again,sizeof(report)))std::_Exit(139);
 uint32_t bytes[4]={0xdeadbeef,0xdeadbeef,0xdeadbeef,0xdeadbeef},written=99;
 if(copy_ticket(bridge,ticket,report.revision,bytes,16,&written).native_error==OK || written || bytes[0]!=0xdeadbeef || release_ticket(bridge,ticket).native_error!=ERR_BUSY)std::_Exit(140);
 GDExtensionQualificationBudgetV1 budget{abi,4,5000,0};
 for(unsigned i=0;i<1000 && report.state!=8;++i) {require(observe_ticket(bridge,ticket,&budget,&report).native_error,"observe");if(report.state==5 || report.state==6 || report.state==7){std::fprintf(stderr,"QB1_TICKET_FAILURE state=%u phase=%u error=%d facts=%u known=%u possible=%u\n",report.state,report.phase,report.native_error,report.fact_count,report.known_count,report.possible_count);std::_Exit(141);}if(report.state!=8)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 if(report.state!=8 || report.owned_size!=16 || report.possible_count || !report.readback_request)std::_Exit(142);
 GDExtensionQualificationFactV1 facts[64]{};uint32_t count=0;require(facts_ticket(bridge,ticket,report.revision,0,64,facts,&count).native_error,"facts");if(count!=report.fact_count || count<4)std::_Exit(143);
 for(uint32_t i=0;i<count;++i)if(facts[i].completion!=2 || facts[i].issue!=1 || !facts[i].native_serial || !facts[i].completion_serial || !facts[i].backend_queue)std::_Exit(144);
 uint32_t stale=99;if(facts_ticket(bridge,ticket,report.revision-1,0,64,facts,&stale).native_error==OK || stale)std::_Exit(145);
 require(copy_ticket(bridge,ticket,report.revision,bytes,16,&written).native_error,"copy");if(written!=16 || bytes[0]!=7 || bytes[1]!=8 || bytes[2]!=9 || bytes[3]!=10)std::_Exit(146);
 require(release_ticket(bridge,ticket).native_error,"ticket_ack");if(release_ticket(bridge,ticket).native_error==OK)std::_Exit(147);
 for(uint64_t set:sets){const void *args[]={&set};method("RenderingDevice","free_rid",2722037293,rd,args);}
 for(unsigned i=0;i<2;++i) {
  GDExtensionNativeUnitRetireRequestV1 retire_request{abi,units[i].unit,3+i,0};GDExtensionNativeUnitRetireRefV1 cell{};GDExtensionNativeUnitRetireReportV1 result{};require(unit_retire(bridge,&retire_request,&cell,&result).native_error,"unit_retire");
  for(unsigned p=0;p<1000 && result.state==0;++p){require(unit_observe(bridge,cell,&budget,&result).native_error,"unit_observe");if(result.state==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));}if(result.state!=1 || result.freed_mask!=3)std::_Exit(148);require(unit_release(bridge,cell).native_error,"unit_ack");
 }
 GDExtensionStorageRetireRequestV1 retirement{abi,backing.backing,backing.rid,backing.backing_serial,3,0,41,2,0};GDExtensionStorageRetireRefV1 cell{};GDExtensionStorageRetireReportV1 physical{};require(storage_retire(bridge,&retirement,&cell,&physical).native_error,"storage_retire");
 for(unsigned p=0;p<1000 && physical.state==0;++p){require(storage_observe(bridge,cell,&budget,&physical).native_error,"storage_observe");if(physical.state==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));}if(physical.state!=1 || !physical.event_seq)std::_Exit(149);require(storage_release(bridge,cell).native_error,"storage_ack");
 require(close_bridge(bridge).native_error,"close");require(release_session(session).native_error,"session");require(release_context(c).native_error,"context");
 std::fprintf(stderr,"QB1_TICKET_NATIVE_COMPLETE actual_main=1 same_ticket=1 facts=%u readback=7,8,9,10 exact_bytes=16 native_sources=1 physical_free=1 ack=1\n",count);return OK;
}

void initialize(void *userdata, GDExtensionInitializationLevel level) {
 if (userdata != &state) { std::_Exit(92); }
 GDExtensionDeviceExitInitializationLevelContextRef frame{};
 require(borrow_init(&abi, library, level, &frame), "borrow_init");
 if (level == GDEXTENSION_INITIALIZATION_SCENE && !PROBE_SKIP_READY) {
  GDExtensionDeviceExitGuardedLibraryRef lib{};GDExtensionDeviceExitRegisteredCleanupRef terminal{};
  require(borrow_library(&abi,library,&lib),"borrow_library");
  require(borrow_cleanup(&abi,frame,1,&terminal),"borrow_binding_cleanup");
  require(acquire_hold(&abi,lib,terminal,&code_hold),"acquire_hold");
  require(install_ready(&abi, frame, ready, userdata), "register_ready");
 }
 require(after_init(&abi, frame), "after_init");
 std::fprintf(stderr, "PHS1_EARLY_ACK tag=%s level=%d\n", PROBE_TAG, int(level));
}
void deinitialize(void *userdata, GDExtensionInitializationLevel level) {
 if (userdata != &state) { std::_Exit(93); }
 GDExtensionDeviceExitDeinitLevelContextRef frame{};
 require(borrow_deinit(&abi, library, level, &frame), "borrow_deinit");
 require(begin_user(&abi, guard, frame), "begin_user");
 std::fprintf(stderr, "PHS1_USER_BODY tag=%s level=%d\n", PROBE_TAG, int(level));
 require(after_user(&abi, frame), "after_user");
 std::fprintf(stderr, "PHS1_BINDING_BODY tag=%s level=%d\n", PROBE_TAG, int(level));
 if (level==GDEXTENSION_INITIALIZATION_SCENE) {
  GDExtensionDeviceRegistryContextRef c{};GDExtensionOriginalDeviceSessionRef s{};uint64_t id=0;
  if(!bind_main(&abi,expired_ready,&c,&s,&id).native_error || c.handle_serial || s.handle_serial || id)std::_Exit(104);
  require(release_hold(&abi,code_hold),"release_hold");
 }
 require(after_binding(&abi, frame), "after_binding");
}
int32_t preflight(GDExtensionDeviceExitDeinitPreflightContextRef frame, void *userdata) {
 if (userdata != &state) { return 30; }
 GDExtensionDeviceExitExitDiagnosticV1 result{};
 result.abi = {1, sizeof(result), 0};
 result.level = UINT32_MAX;
 require(publish(&abi, guard, frame, &result), "publish");
 return 0;
}
template<class T> bool load(GDExtensionInterfaceGetProcAddress get, const char *name, T &out) {
 out = reinterpret_cast<T>(get(name));
 return out != nullptr;
}
}

extern "C" __attribute__((visibility("default"))) GDExtensionBool phs1_probe_init(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr owner, GDExtensionInitialization *result) {
 // Freeze an all-or-nothing table before guard enrollment or early effects.
 if (!load(get,"device_session_qualification_rt_configure_v1",rt_configure) || !load(get,"device_session_qualification_rt_reserve_v1",rt_reserve) || !load(get,"device_session_qualification_rt_activate_v1",rt_activate) || !load(get,"device_session_qualification_rt_observe_v1",rt_observe) || !load(get,"device_session_qualification_rt_release_v1",rt_release)) {std::fprintf(stderr,"QRT_PREFLIGHT_RED effects=0\n");std::_Exit(86);}

 if (!load(get,"device_session_qualification_record_v1",record_ticket) ||
!load(get,"device_session_qualification_observe_v1",observe_ticket) ||
!load(get,"device_session_qualification_facts_v1",facts_ticket) ||
!load(get,"device_session_qualification_copy_v1",copy_ticket) ||
!load(get,"device_session_qualification_release_v1",release_ticket) ||
!load(get,"global_get_singleton",global_singleton) ||
!load(get,"callable_custom_create2",callable_create) ||
!load(get,"string_name_new_with_utf8_chars",name_new) ||
!load(get,"variant_get_ptr_destructor",type_destroy) ||
!load(get,"variant_get_ptr_constructor",type_construct) ||
!load(get,"classdb_construct_object2",construct_object) ||
!load(get,"classdb_get_method_bind",get_method) ||
!load(get,"object_method_bind_ptrcall",call_method) ||
!load(get,"get_variant_from_type_constructor",from_type) ||
!load(get,"variant_new_nil",variant_nil) ||
!load(get,"variant_destroy",variant_destroy) ||
!load(get,"array_set_typed",set_array_type) ||
!load(get,"variant_get_ptr_builtin_method",get_builtin) ||
!load(get,"object_get_instance_from_id",object_from_id) ||
     !load(get,"device_session_qualification_storage_create_v1",storage_create) ||
     !load(get,"device_session_qualification_storage_retire_v1",storage_retire) ||
     !load(get,"device_session_qualification_storage_retire_observe_v1",storage_observe) ||
     !load(get,"device_session_qualification_storage_retire_release_v1",storage_release) ||
     !load(get,"device_session_qualification_native_unit_create_v1",unit_create) ||
     !load(get,"device_session_qualification_native_unit_retire_v1",unit_retire) ||
     !load(get,"device_session_qualification_native_unit_retire_observe_v1",unit_observe) ||
     !load(get,"device_session_qualification_native_unit_retire_release_v1",unit_release) ||
     !load(get,"device_exit_bootstrap_library_v1",bootstrap_library) ||
     !load(get,"device_exit_retain_library_view_v1",retain_library_view) ||
     !load(get,"device_exit_copy_library_snapshot_v1",copy_library_snapshot) ||
     !load(get,"device_exit_release_library_view_v1",release_library_view) ||
     !load(get,"device_session_bind_main_scene_ready_v1",bind_main) ||
     !load(get,"device_session_qualification_open_v1",open_bridge) ||
     !load(get,"device_session_qualification_close_v1",close_bridge) ||
     !load(get,"device_exit_borrow_guarded_library_v1",borrow_library) ||
     !load(get,"device_exit_borrow_level_cleanup_v1",borrow_cleanup) ||
     !load(get,"device_exit_acquire_code_hold_v1",acquire_hold) ||
     !load(get,"device_exit_release_code_hold_v1",release_hold) ||
     !load(get,"device_session_release_context_v1",release_context) ||
     !load(get,"device_session_release_session_v1",release_session) ||
     !load(get, "device_exit_register_guard_v1", enroll) ||
     !load(get, "device_exit_borrow_initialize_level_v1", borrow_init) ||
     !load(get, "device_exit_after_binding_initialize_v1", after_init) ||
     !load(get, "device_exit_borrow_deinit_level_v1", borrow_deinit) ||
     !load(get, "device_exit_begin_user_terminator_v1", begin_user) ||
     !load(get, "device_exit_after_user_terminator_v1", after_user) ||
     !load(get, "device_exit_after_binding_deinit_v1", after_binding) ||
     !load(get, "device_exit_publish_exit_result_v1", publish) ||
     !load(get, "device_exit_register_scene_ready_v1", install_ready) ||
     !load(get, "device_exit_scene_ready_schedule_object_cleanup_v1", schedule_ready) ||
     !load(get, "device_session_bind_scene_ready_v1", bind_ready) ||
     !load(get, "device_exit_scene_ready_abort_v1", abort_ready)) { std::fprintf(stderr,"QB1_AUTHORITY_PREFLIGHT_RED effects=0\n"); std::_Exit(86); }
 storage_create=&wrapped_storage_create;storage_retire=&wrapped_storage_retire;unit_create=&wrapped_unit_create;unit_retire=&wrapped_unit_retire;
 record_ticket=&wrapped_record;
 GDExtensionDeviceExitLibraryCapability capability{};GDExtensionDeviceExitLibraryViewRef view{};
 require(bootstrap_library(&abi,owner,&capability),"bootstrap_library");require(retain_library_view(&abi,capability,&view),"retain_view");
 GDExtensionDeviceExitLibrarySnapshot snapshot{};snapshot.abi={1,sizeof(snapshot),0};require(copy_library_snapshot(view,&snapshot),"copy_snapshot");library_generation=snapshot.library_generation;require(release_library_view(&abi,view),"release_view");
 library = owner;
 *result = {};
 result->minimum_initialization_level = GDEXTENSION_INITIALIZATION_SCENE;
 result->userdata = &state;
 result->initialize = initialize;
 result->deinitialize = deinitialize;
 GDExtensionDeviceExitGuardRegistrationV1 registration{};
 registration.abi = {1, sizeof(registration), 0};
 registration.binding_protocol = 1;
 registration.level_bitmap = registration.user_cleanup_bitmap = registration.binding_cleanup_bitmap = 12;
 registration.entry_classes = GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1;
 registration.expected_initialization = *result;
 registration.user_initializer = initialize;
 registration.user_terminator = deinitialize;
 registration.preflight = preflight;
 registration.preflight_userdata = &state;
 return enroll(library, &registration, &guard) == 0;
}

__attribute__((destructor)) static void unloaded() {
 std::fprintf(stderr, "PHS1_DSO_UNLOAD tag=%s\n", PROBE_TAG);
}
