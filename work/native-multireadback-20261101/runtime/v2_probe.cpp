#ifndef TICKET_MODE
#define TICKET_MODE 1
#endif
#include "gdextension_interface_v2.pinned.h"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <fstream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <utility>
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

#ifndef TICKET_MODE
#define TICKET_MODE 1
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

GDExtensionInterfaceDeviceSessionQualificationPrepareUploadsV2 prepare_ticket;
GDExtensionInterfaceDeviceSessionQualificationUploadV1 upload_ticket;
GDExtensionInterfaceDeviceSessionQualificationRecordPreparedV2 record_prepared_ticket;
GDExtensionInterfaceDeviceSessionQualificationCancelV1 cancel_ticket;

GDExtensionInterfaceDeviceSessionQualificationObserveV2 observe_ticket;
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
struct alignas(8) Opaque {unsigned char bytes[64]={};};
void method(const char *klass,const char *name,uint64_t hash,void *object,const GDExtensionConstTypePtr *args,void *out=nullptr) {
 Opaque k,n;name_new(&k,klass);name_new(&n,name);auto m=get_method(&k,&n,hash);if(!m)std::_Exit(130);call_method(m,object,args,out);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&k);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&n);
}
uint64_t set_create(void *rd,uint64_t shader,const uint64_t *buffers,uint32_t count,int64_t set_index) {
 Opaque klass,array,nil,append;name_new(&klass,"RDUniform");
 type_construct(GDEXTENSION_VARIANT_TYPE_ARRAY,0)(&array,nullptr);variant_nil(&nil);
 set_array_type(&array,GDEXTENSION_VARIANT_TYPE_OBJECT,&klass,&nil);
 name_new(&append,"append");auto fn=get_builtin(GDEXTENSION_VARIANT_TYPE_ARRAY,&append,3316032543);if(!fn)std::_Exit(132);
 for(uint32_t i=0;i<count;++i){
  void *uniform=construct_object(&klass);if(!uniform)std::_Exit(131);
  int64_t kind=8,binding=i;const void *a[]={&kind};method("RDUniform","set_uniform_type",1664894931,uniform,a);
  a[0]=&binding;method("RDUniform","set_binding",1286410249,uniform,a);a[0]=&buffers[i];method("RDUniform","add_id",2722037293,uniform,a);
  Opaque object_variant;from_type(GDEXTENSION_VARIANT_TYPE_OBJECT)(&object_variant,&uniform);a[0]=&object_variant;fn(&array,a,nullptr,1);variant_destroy(&object_variant);
 }
 uint64_t set=0;const void *args[]={&array,&shader,&set_index};method("RenderingDevice","uniform_set_create",2280795797,rd,args,&set);
 variant_destroy(&nil);type_destroy(GDEXTENSION_VARIANT_TYPE_ARRAY)(&array);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&klass);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&append);
 if(!set)std::_Exit(133);return set;
}
GDExtensionInterfaceDeviceSessionQualificationRtConfigureV1 rt_configure;
GDExtensionInterfaceDeviceSessionQualificationRtReserveV1 rt_reserve;
GDExtensionInterfaceDeviceSessionQualificationRtActivateV1 rt_activate;
GDExtensionInterfaceDeviceSessionQualificationRtObserveV1 rt_observe;
GDExtensionInterfaceDeviceSessionQualificationRtReleaseV1 rt_release;
uint64_t rt_operation=1;bool rt_configured=false,rt_abort_tested=false;
GDExtensionDeviceSessionResultV1 invoke_rt(GDExtensionQualificationBridgeRefV1 bridge,uint32_t kind,const void *request,uint32_t request_size,const void *payload,uint64_t payload_size,void *out,uint32_t out_size,GDExtensionQualificationUploadSegmentV1 *segments=nullptr,uint32_t segment_capacity=0) {
 memset(out,0,out_size);if(segment_capacity)memset(segments,0,segment_capacity*sizeof(*segments));
 if(!rt_configured){GDExtensionQualificationRtConfigV1 config{{1,sizeof(config),0},1,448,344,4,65536};require(rt_configure(bridge,&config).native_error,"rt_configure");rt_configured=true;}
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

static_assert(sizeof(GDExtensionQualificationRecordV2)==448);
static_assert(sizeof(GDExtensionQualificationReportV2)==328);
static_assert(sizeof(GDExtensionQualificationReadbackRangeV1)==48);
static_assert(sizeof(GDExtensionQualificationReadbackObservationV1)==56);
GDExtensionDeviceSessionResultV1 wrapped_prepare(GDExtensionQualificationBridgeRefV1 b,const GDExtensionQualificationUploadPrepareV1 *q,const GDExtensionQualificationUploadPlanV1 *p,uint32_t n,GDExtensionQualificationTicketRefV1 *t,GDExtensionQualificationReportV2 *o){struct Pair{GDExtensionQualificationTicketRefV1 ref;GDExtensionQualificationReportV2 report;}pair{};static_assert(sizeof(pair)==344);auto result=invoke_rt(b,11,q,sizeof(*q),p,uint64_t(n)*sizeof(*p),&pair,sizeof(pair));*t=pair.ref;*o=pair.report;return result;}
GDExtensionDeviceSessionResultV1 wrapped_upload(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,const GDExtensionQualificationUploadRequestV1 *q,const void *p,uint32_t n,GDExtensionQualificationUploadSegmentV1 *s,uint32_t c,GDExtensionQualificationUploadReportV1 *o){std::vector<uint8_t> payload(16+size_t(n));memcpy(payload.data(),&t,16);memcpy(payload.data()+16,p,n);return invoke_rt(b,8,q,sizeof(*q),payload.data(),payload.size(),o,sizeof(*o),s,c);}
GDExtensionDeviceSessionResultV1 wrapped_prepared(GDExtensionQualificationBridgeRefV1 b,GDExtensionQualificationTicketRefV1 t,const GDExtensionQualificationRecordV2 *q,GDExtensionQualificationReportV2 *o){return invoke_rt(b,10,q,sizeof(*q),&t,sizeof(t),o,sizeof(*o));}
void check(bool valid,const char *where){if(!valid){std::fprintf(stderr,"MULTI_V2_ASSERT where=%s mode=%d\n",where,TICKET_MODE);std::_Exit(180);}}
std::vector<uint8_t> read_file(const char *path){check(path,"input_path");std::ifstream input(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});check(!bytes.empty(),"input_bytes");return bytes;}
int32_t ready(void *userdata,GDExtensionDeviceExitSceneReadyContextRef context) {
 check(userdata==&state,"ready_userdata");state.calls++;expired_ready=context;
 GDExtensionDeviceRegistryContextRef c{};GDExtensionOriginalDeviceSessionRef session{};uint64_t id=0;
 require(bind_main(&abi,context,&c,&session,&id).native_error,"bind_main");
 GDExtensionQualificationBridgeRefV1 bridge{};require(open_bridge(&abi,context,c,session,code_hold,&bridge).native_error,"open_bridge");
 const uint64_t sizes[]={24,16,4,16};
 GDExtensionStorageBackingV1 backings[4]{};
 for(uint32_t i=0;i<4;++i){GDExtensionStorageCreateRequestV1 q{abi,sizes[i],3,i,41,1,0};require(storage_create(bridge,&q,&backings[i]).native_error,"storage_create");check(backings[i].state==1 && backings[i].byte_size==sizes[i] && backings[i].backing.handle_serial,"storage_original");}
 GDExtensionNativeUnitBackingV1 units[2]{};uint8_t container_digests[2][32]{};
 const char *paths[]={std::getenv("MULTI_V2_CAPTURE_CONTAINER"),std::getenv("MULTI_V2_BODY_CONTAINER")};
 for(uint32_t i=0;i<2;++i){
  const auto bytes=read_file(paths[i]);GDExtensionNativeUnitCreateRequestV1 q{};q.abi=abi;q.library_generation=library_generation;q.preparation_generation=21;q.operation_serial=i+1;q.container_byte_count=bytes.size();q.record_slot=2;q.physical_unit=i;CC_SHA256(bytes.data(),bytes.size(),q.container_digest);memcpy(container_digests[i],q.container_digest,32);
  require(unit_create(bridge,&q,bytes.data(),bytes.size(),&units[i]).native_error,"unit_create");check(units[i].state==1 && units[i].known_mask==3 && !units[i].possible_mask,"unit_original");
 }
 void *rd=object_from_id(id);check(rd,"main_rd");
 const uint64_t output_buffers[]={backings[0].rid,backings[1].rid,backings[2].rid};
 const uint64_t input_buffers[]={backings[3].rid};
 uint64_t sets[2][2]{};
 for(uint32_t i=0;i<2;++i){sets[i][0]=set_create(rd,units[i].shader_rid,output_buffers,3,0);sets[i][1]=set_create(rd,units[i].shader_rid,input_buffers,1,1);}
 GDExtensionQualificationRecordV2 request{};request.abi={2,sizeof(GDExtensionDeviceExitAbi),0};
 request.record_generation=19;request.request_nonce=23;request.library_generation=library_generation;request.qualification_session_serial=7;request.range_end=4;request.attempt=1;request.sample_count=4;request.body_groups_x=1;
 request.capture_pipeline=units[0].pipeline_rid;request.capture_set=sets[0][0];request.capture_set1=sets[0][1];request.capture_set_count=2;
 request.body_pipeline=units[1].pipeline_rid;request.body_set=sets[1][0];request.body_set1=sets[1][1];request.body_set_count=2;
 request.readback_count=3;request.readback_size=44;
 const uint32_t roles[]={GDEXTENSION_QUALIFICATION_READBACK_METHOD_RAW_RESULTS_V1,GDEXTENSION_QUALIFICATION_READBACK_METHOD_FAULT_PLANE_V1,GDEXTENSION_QUALIFICATION_READBACK_METHOD_RESULT_EPILOGUE_V1};
 uint32_t offset=0;
 for(uint32_t i=0;i<3;++i){request.readbacks[i]={backings[i].rid,41,backings[i].backing_serial,0,sizes[i],offset,roles[i]};offset+=sizes[i];}
 const uint32_t push[]={0,4,0,0};memcpy(request.capture_push,push,16);memcpy(request.body_push,push,16);
 const char identity[]="Godot native V2 Metal control: four original E05 backings, three same-ticket ranges, two sets; not an ANGE request";
 CC_SHA256(identity,sizeof(identity)-1,request.request_digest);CC_SHA256(container_digests,sizeof(container_digests),request.unit_digest);
 const auto capture_source=read_file(std::getenv("MULTI_V2_CAPTURE_SOURCE"));const auto body_source=read_file(std::getenv("MULTI_V2_BODY_SOURCE"));
 CC_SHA256(capture_source.data(),capture_source.size(),request.capture_map_digest);CC_SHA256(body_source.data(),body_source.size(),request.body_map_digest);
 GDExtensionQualificationUploadPrepareV1 preparation{};preparation.abi=abi;preparation.record_generation=request.record_generation;preparation.request_nonce=request.request_nonce;preparation.library_generation=library_generation;preparation.qualification_session_serial=request.qualification_session_serial;preparation.parent_serial=91;preparation.reservation_serial=97;preparation.attempt=1;preparation.upload_count=4;preparation.max_segments=4;preparation.expected_result_bytes=44;memcpy(preparation.request_digest,request.request_digest,32);memcpy(preparation.unit_digest,request.unit_digest,32);
 const uint32_t raw[]={0xa5a5a5a5,0xa5a5a5a5,0xa5a5a5a5,0xa5a5a5a5,0xa5a5a5a5,0xa5a5a5a5};
 const uint32_t fault[]={0,0,0,0},epilogue[]={0},parameters[]={100,200,0,1};
 const void *payloads[]={raw,fault,epilogue,parameters};
 const uint32_t upload_roles[]={GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RAW_RESULTS_V1,GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_FAULT_PLANE_V1,GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RESULT_EPILOGUE_V1,GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RAW_PARAMETERS_V1};
 static_assert(GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RAW_RESULTS_V1==15 && GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_FAULT_PLANE_V1==9 && GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RESULT_EPILOGUE_V1==16 && GDEXTENSION_QUALIFICATION_UPLOAD_ROLE_METHOD_RAW_PARAMETERS_V1==14);
 GDExtensionQualificationUploadPlanV1 plans[4]{};
 for(uint32_t i=0;i<4;++i){auto &p=plans[i];p.abi=abi;p.upload_index=i;p.role=upload_roles[i];p.owner_kind=GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1;p.rid=backings[i].rid;p.backing_serial=backings[i].backing_serial;p.byte_size=sizes[i];p.e05_backing=backings[i].backing;CC_SHA256(payloads[i],sizes[i],p.payload_digest);}
 GDExtensionQualificationTicketRefV1 ticket{},repeat{};GDExtensionQualificationReportV2 report{},again{};
 require(prepare_ticket(bridge,&preparation,plans,4,&ticket,&report).native_error,"prepare_v2");check(ticket.handle_serial && report.abi.abi_version==2 && !report.state && !report.fact_count,"prepared_original");
 require(prepare_ticket(bridge,&preparation,plans,4,&repeat,&again).native_error,"prepare_v2_replay");check(!memcmp(&ticket,&repeat,sizeof(ticket)) && !memcmp(&report,&again,sizeof(report)),"prepare_exact_replay");
 GDExtensionQualificationUploadRequestV1 uploads[4]{};
 uint64_t revision=report.revision;
 for(uint32_t i=0;i<4;++i){
  auto &q=uploads[i];const auto &p=plans[i];q.abi=abi;q.record_generation=request.record_generation;q.parent_serial=preparation.parent_serial;q.rid=p.rid;q.backing_serial=p.backing_serial;q.byte_offset=p.byte_offset;q.byte_size=p.byte_size;q.upload_index=i;q.role=p.role;q.owner_kind=p.owner_kind;q.e05_backing=p.e05_backing;memcpy(q.payload_digest,p.payload_digest,32);q.expected_revision=revision;
  GDExtensionQualificationUploadSegmentV1 segments[1]{};GDExtensionQualificationUploadReportV1 uploaded{};
  for(uint32_t pass=0;pass<1000;++pass){
   require(upload_ticket(bridge,ticket,&q,payloads[i],sizes[i],segments,1,&uploaded).native_error,"upload_v1_same_v2_ticket");
   check(!uploaded.possible_count && uploaded.record_generation==request.record_generation && uploaded.parent_serial==preparation.parent_serial && uploaded.upload_index==i,"upload_original_identity");
   if(uploaded.state==1)break;
   check(uploaded.state==4,"upload_known_continuation");q.expected_revision=uploaded.revision;std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  check(uploaded.state==1 && uploaded.known_count && uploaded.out_written==1 && uploaded.appended_begin==0 && uploaded.appended_end==sizes[i] && uploaded.unappended_begin==sizes[i] && segments[0].state==1 && segments[0].backing_serial==backings[i].backing_serial && segments[0].byte_begin==0 && segments[0].byte_end==sizes[i],"upload_exact_segments");
  revision=uploaded.revision;
 }
 // This exact owner observation cannot be a completed readback before record.
 const GDExtensionQualificationBudgetV1 prefix_budget{abi,1,1000,0};
 require(observe_ticket(bridge,ticket,&prefix_budget,&report).native_error,"observe_uploaded_prefix");
 uint32_t bytes[11];for(auto &word:bytes)word=0xdeadbeef;uint32_t written=99;
 auto early=copy_ticket(bridge,ticket,report.revision,bytes,sizeof(bytes),&written);check(early.native_error==ERR_BUSY && !written && bytes[0]==0xdeadbeef,"early_copy_busy");
 check(release_ticket(bridge,ticket).native_error==ERR_BUSY,"early_release_busy");
 if(TICKET_MODE==1){
  require(record_prepared_ticket(bridge,ticket,&request,&report).native_error,"record_prepared_v2");
  require(record_prepared_ticket(bridge,ticket,&request,&again).native_error,"record_prepared_v2_replay");check(!memcmp(&report,&again,sizeof(report)),"record_exact_replay");
 } else {
  require(cancel_ticket(bridge,ticket).native_error,"cancel_uploaded_prefix");require(cancel_ticket(bridge,ticket).native_error,"cancel_replay");
  GDExtensionQualificationUploadSegmentV1 stopped_segments[1]{};GDExtensionQualificationUploadReportV1 stopped_report{};
  auto stopped=upload_ticket(bridge,ticket,&uploads[0],payloads[0],sizes[0],stopped_segments,1,&stopped_report);check(stopped.native_error==ERR_BUSY && stopped.reason==16,"cancel_closes_upload");
  stopped=record_prepared_ticket(bridge,ticket,&request,&again);check(stopped.native_error==ERR_BUSY && stopped.reason==16,"cancel_closes_record");
 }
 const GDExtensionQualificationBudgetV1 budget{abi,4,5000,0};
 bool done=false;
 for(uint32_t pass=0;pass<5000;++pass){
  require(observe_ticket(bridge,ticket,&budget,&report).native_error,"observe_v2");
  done=TICKET_MODE==1?report.state==8:(report.state==7 && report.phase==6);
  if(done)break;
  check(report.state!=5 && report.state!=6 && report.state!=7,"no_failed_unknown_prefix");std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 check(done && !report.native_error && !report.native_code && !report.possible_count && report.record_generation==request.record_generation && report.request_nonce==request.request_nonce && !memcmp(report.request_digest,request.request_digest,32) && !memcmp(report.unit_digest,request.unit_digest,32),"same_ticket_terminal");
 check(report.fact_count && report.fact_count<=4096,"facts_bound");
 std::vector<GDExtensionQualificationFactV1> facts(report.fact_count);uint32_t count=0;
 require(facts_ticket(bridge,ticket,report.revision,0,facts.size(),facts.data(),&count).native_error,"same_ticket_facts");check(count==report.fact_count,"all_facts");
 uint32_t graph_readbacks=0;uint64_t readback_bytes[3]{};
 std::vector<std::pair<uint64_t,uint64_t>> readback_spans[3];
 for(const auto &fact:facts){
  check(fact.completion==2 && fact.issue==1 && !fact.error && fact.native_serial && fact.completion_serial && fact.backend_queue,"native_complete_exact_sources");
  if(fact.semantic_kind==3){++graph_readbacks;bool original=false;for(uint32_t i=0;i<3;++i){const auto &r=request.readbacks[i];if(fact.source_backing_serial==r.observed_backing_serial && fact.source_offset>=r.readback_offset && fact.source_size<=r.readback_size && fact.source_offset-r.readback_offset<=r.readback_size-fact.source_size){readback_bytes[i]+=fact.source_size;readback_spans[i].push_back({fact.source_offset,fact.source_offset+fact.source_size});original=true;break;}}check(original,"readback_fact_original_range");}
 }
 if(TICKET_MODE==1){
  check(report.readback_count==3 && report.readback_size==44 && report.owned_size==44 && graph_readbacks>=3,"three_actual_results");
  for(uint32_t i=0;i<3;++i){
   check(!memcmp(&report.readbacks[i].source,&request.readbacks[i],sizeof(request.readbacks[i])) && report.readbacks[i].readback_request && readback_bytes[i]==sizes[i],"actual_async_range");
   for(uint32_t j=0;j<i;++j)check(report.readbacks[i].readback_request!=report.readbacks[j].readback_request,"distinct_original_async_serials");
   std::sort(readback_spans[i].begin(),readback_spans[i].end());uint64_t cursor=request.readbacks[i].readback_offset;
   for(const auto &span:readback_spans[i]){check(span.first==cursor && span.second>span.first,"gapless_original_readback_facts");cursor=span.second;}
   check(cursor==request.readbacks[i].readback_offset+sizes[i],"complete_original_readback_facts");
  }
  require(copy_ticket(bridge,ticket,report.revision,bytes,sizeof(bytes),&written).native_error,"copy_exact_44");
  const uint32_t expected[]={100,101,102,103,0xa5a5a5a5,0xa5a5a5a5,0,0,0,0,1};check(written==44 && !memcmp(bytes,expected,sizeof(expected)),"exact_outputs_guards_fault_epilogue");
  check(cancel_ticket(bridge,ticket).native_error==ERR_BUSY,"completed_result_immutable");
 } else {
  check(!report.owned_size && !graph_readbacks,"cancel_prefix_has_no_readback");written=99;check(copy_ticket(bridge,ticket,report.revision,bytes,sizeof(bytes),&written).native_error==ERR_BUSY && !written,"cancel_has_no_result");
 }
 uint32_t stale=99;check(facts_ticket(bridge,ticket,report.revision-1,0,facts.size(),facts.data(),&stale).native_error!=OK && !stale,"stale_revision_refused");
 require(release_ticket(bridge,ticket).native_error,"ticket_physical_ack");check(release_ticket(bridge,ticket).native_error!=OK,"ticket_ack_once");
 for(const auto &phase:sets)for(uint64_t set:phase){const void *args[]={&set};method("RenderingDevice","free_rid",2722037293,rd,args);}
 for(uint32_t i=0;i<2;++i){
  GDExtensionNativeUnitRetireRequestV1 q{abi,units[i].unit,3+i,0};GDExtensionNativeUnitRetireRefV1 cell{};GDExtensionNativeUnitRetireReportV1 physical{};require(unit_retire(bridge,&q,&cell,&physical).native_error,"unit_retire");
  for(uint32_t pass=0;pass<5000 && physical.state==0;++pass){require(unit_observe(bridge,cell,&budget,&physical).native_error,"unit_observe");if(!physical.state)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(physical.state==1 && physical.freed_mask==3 && !physical.unresolved_mask && physical.shader_free_event_seq && physical.pipeline_free_event_seq,"unit_actual_free");require(unit_release(bridge,cell).native_error,"unit_physical_ack");
 }
 for(uint32_t i=0;i<4;++i){
  GDExtensionStorageRetireRequestV1 q{abi,backings[i].backing,backings[i].rid,backings[i].backing_serial,3,i,41,2,0};GDExtensionStorageRetireRefV1 cell{};GDExtensionStorageRetireReportV1 physical{};require(storage_retire(bridge,&q,&cell,&physical).native_error,"storage_retire");
  if(!physical.state)check(storage_release(bridge,cell).native_error==ERR_BUSY,"physical_early_ack_busy");
  for(uint32_t pass=0;pass<5000 && physical.state==0;++pass){require(storage_observe(bridge,cell,&budget,&physical).native_error,"storage_observe");if(!physical.state)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
  check(physical.state==1 && physical.return_status==1 && physical.event_seq && !physical.native_error,"storage_actual_free");require(storage_release(bridge,cell).native_error,"storage_physical_ack");
 }
 require(close_bridge(bridge).native_error,"bridge_close");require(release_session(session).native_error,"session_release");require(release_context(c).native_error,"context_release");
 std::fprintf(stderr,"MULTI_V2_NATIVE mode=%d actual_main=1 same_ticket=1 set_count=2 upload_plans=4 readbacks=%u actual_async_distinct=%d exact_bytes=%u guards=%d native_complete=1 facts=%u physical_free=1 ack=1 ange_product_acceptance=0\n",TICKET_MODE,report.readback_count,TICKET_MODE==1?1:0,written,TICKET_MODE==1?1:0,count);
 return OK;
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

 if (!load(get,"device_session_qualification_prepare_uploads_v2",prepare_ticket) ||
!load(get,"device_session_qualification_upload_v1",upload_ticket) ||
!load(get,"device_session_qualification_record_prepared_v2",record_prepared_ticket) ||
!load(get,"device_session_qualification_cancel_v1",cancel_ticket) ||
!load(get,"device_session_qualification_observe_v2",observe_ticket) ||
!load(get,"device_session_qualification_facts_v1",facts_ticket) ||
!load(get,"device_session_qualification_copy_v1",copy_ticket) ||
!load(get,"device_session_qualification_release_v1",release_ticket) ||
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
 prepare_ticket=&wrapped_prepare;upload_ticket=&wrapped_upload;record_prepared_ticket=&wrapped_prepared;
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
