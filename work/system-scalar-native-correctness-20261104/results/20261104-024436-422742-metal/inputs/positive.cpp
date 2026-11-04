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
GDExtensionInterfaceDeviceSessionQualificationResourceRetireObserveV1 typed_observe;
GDExtensionInterfaceDeviceSessionQualificationResourceRetireReleaseV1 typed_release;
GDExtensionInterfaceDeviceSessionQualificationRtConfigureV1 rt_configure;
GDExtensionInterfaceDeviceSessionQualificationRtReserveV1 rt_reserve;
GDExtensionInterfaceDeviceSessionQualificationRtActivateV1 rt_activate;
GDExtensionInterfaceDeviceSessionQualificationRtObserveV1 rt_observe;
GDExtensionInterfaceDeviceSessionQualificationRtReleaseV1 rt_release;
uint64_t rt_operation=1;bool rt_configured=false,rt_abort_tested=false;
GDExtensionDeviceSessionResultV1 invoke_rt(GDExtensionQualificationBridgeRefV1 bridge,uint32_t kind,const void *request,uint32_t request_size,const void *payload,uint64_t payload_size,void *out,uint32_t out_size,GDExtensionQualificationUploadSegmentV1 *segments=nullptr,uint32_t segment_capacity=0) {
 memset(out,0,out_size);if(segment_capacity)memset(segments,0,segment_capacity*sizeof(*segments));
 if(!rt_configured){GDExtensionQualificationRtConfigV1 config{{1,sizeof(config),0},1,448,344,8,65536};require(rt_configure(bridge,&config).native_error,"rt_configure");rt_configured=true;}
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


#ifndef SCALAR_EXPECTED_WORD
#define SCALAR_EXPECTED_WORD 0x40400000u
#endif
GDExtensionInterfaceDeviceSessionSystemScalarRecordV1 scalar_record;
GDExtensionInterfaceDeviceSessionSystemScalarObserveV1 scalar_observe;
GDExtensionInterfaceDeviceSessionSystemScalarFactsV1 scalar_facts;
GDExtensionInterfaceDeviceSessionSystemScalarCopyV1 scalar_copy;
GDExtensionInterfaceDeviceSessionSystemScalarCancelV1 scalar_cancel;
GDExtensionInterfaceDeviceSessionSystemScalarReleaseV1 scalar_release;
GDExtensionInterfaceCallableCustomCreate2 make_callable;
GDExtensionInterfaceGlobalGetSingleton get_singleton;
void check(bool valid,const char *where){if(!valid){std::fprintf(stderr,"SYSTEM_SCALAR_ASSERT where=%s\n",where);std::_Exit(180);}}
std::vector<uint8_t> read_file(const char *path){check(path,"input_path");std::ifstream input(path,std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});check(!bytes.empty(),"input_bytes");return bytes;}
GDExtensionMethodBindPtr method(const char *name,uint64_t hash) {
 Opaque cls{},member{};name_new(&cls,"RenderingServer");name_new(&member,name);
 auto bind=get_method(&cls,&member,hash);
 type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&member);type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&cls);
 check(bind,"render_server_method");return bind;
}
GDExtensionObjectPtr render_server() {
 Opaque name{};name_new(&name,"RenderingServer");auto out=get_singleton(&name);
 type_destroy(GDEXTENSION_VARIANT_TYPE_STRING_NAME)(&name);check(out,"render_server_singleton");return out;
}
struct RecordCall {
 GDExtensionQualificationBridgeRefV1 bridge{};
 GDExtensionSystemScalarRecordV1 request{};
 GDExtensionSystemScalarTicketRefV1 ticket{};
 GDExtensionSystemScalarReportV1 report{};
 GDExtensionDeviceSessionResultV1 result{};
 std::atomic<bool> done{false};
};
void record_on_render_thread(void *userdata,const GDExtensionConstVariantPtr *,GDExtensionInt argc,GDExtensionVariantPtr returned,GDExtensionCallError *error) {
 auto *job=static_cast<RecordCall *>(userdata);check(argc==0,"render_callback_arguments");
 GDExtensionBool on_render_thread=false;call_method(method("is_on_render_thread",2240911060),render_server(),nullptr,&on_render_thread);
 check(on_render_thread,"actual_render_thread");
 job->result=scalar_record(job->bridge,&job->request,&job->ticket,&job->report);
 variant_nil(returned);error->error=GDEXTENSION_CALL_OK;
 std::fprintf(stderr,"SYSTEM_SCALAR_RECORD actual_render_thread=1 error=%d ticket=%llu state=%u\n",job->result.native_error,(unsigned long long)job->ticket.handle_serial,job->report.state);
 job->done.store(true,std::memory_order_release);
}
int32_t ready(void *userdata,GDExtensionDeviceExitSceneReadyContextRef context) {
 check(userdata==&state,"ready_userdata");state.calls++;expired_ready=context;
 GDExtensionDeviceRegistryContextRef c{};GDExtensionOriginalDeviceSessionRef session{};uint64_t id=0;
 require(bind_main(&abi,context,&c,&session,&id).native_error,"bind_main");
 GDExtensionQualificationBridgeRefV1 bridge{};require(open_bridge(&abi,context,c,session,code_hold,&bridge).native_error,"open_bridge");
 GDExtensionStorageCreateRequestV1 allocation{abi,4,3,0,41,1,0};GDExtensionStorageBackingV1 backing{};
 require(storage_create(bridge,&allocation,&backing).native_error,"storage_create");
 check(backing.state==1 && backing.byte_size==4 && backing.backing.handle_serial,"storage_original");
 const auto container=read_file(std::getenv("SYSTEM_SCALAR_CONTAINER"));
 GDExtensionNativeUnitCreateRequestV1 create{};create.abi=abi;create.library_generation=library_generation;create.preparation_generation=21;create.operation_serial=1;create.container_byte_count=container.size();create.record_slot=2;create.physical_unit=0;CC_SHA256(container.data(),container.size(),create.container_digest);
 GDExtensionNativeUnitBackingV1 unit{};require(unit_create(bridge,&create,container.data(),container.size(),&unit).native_error,"unit_create");
 check(unit.state==1 && unit.known_mask==3 && !unit.possible_mask,"unit_original");
 RecordCall job;job.bridge=bridge;job.request.abi=abi;job.request.unit=unit;job.request.output=backing;job.request.output_allocation_generation=41;job.request.library_generation=library_generation;job.request.session_serial=7;job.request.record_generation=19;job.request.request_nonce=23;job.request.output_word_count=1;
 Opaque callable{};GDExtensionCallableCustomInfo2 info{};info.callable_userdata=&job;info.token=library;info.call_func=&record_on_render_thread;
 make_callable(&callable,&info);const GDExtensionConstTypePtr args[]={&callable};
 call_method(method("call_on_render_thread",1611583062),render_server(),args,nullptr);
 for(uint32_t pass=0;pass<5000 && !job.done.load(std::memory_order_acquire);++pass)std::this_thread::sleep_for(std::chrono::milliseconds(1));
 check(job.done.load(std::memory_order_acquire),"record_callback_returned");type_destroy(GDEXTENSION_VARIANT_TYPE_CALLABLE)(&callable);
 require(job.result.native_error,"scalar_record");check(job.ticket.registry_instance && job.ticket.handle_serial,"scalar_original_ticket");
 const GDExtensionQualificationBudgetV1 budget{abi,4,5000,0};auto report=job.report;bool done=false;
 uint32_t value=0xdeadbeef,written=99;
 for(uint32_t pass=0;pass<5000;++pass){
  require(scalar_observe(bridge,job.ticket,&budget,&report).native_error,"scalar_observe");
  if(report.state==8){done=true;break;}
  check(report.state!=5 && report.state!=6 && report.state!=7,"scalar_no_failed_unknown");std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 check(done && !report.native_error && !report.native_code && !report.possible_count && report.record_generation==19 && report.request_nonce==23 && report.readback_request && report.source_backing_serial==backing.backing_serial && report.source_offset==0 && report.source_size==4 && report.owned_size==4,"scalar_terminal_identity");
 check(report.fact_count && report.fact_count<=64,"scalar_fact_count");
 std::vector<GDExtensionQualificationFactV1> facts(report.fact_count);uint32_t count=0;require(scalar_facts(bridge,job.ticket,report.revision,0,facts.size(),facts.data(),&count).native_error,"scalar_facts");check(count==facts.size(),"scalar_all_facts");
 uint32_t body_facts=0,copy_facts=0;uint64_t copy_bytes=0;
 for(const auto &fact:facts){
  check(fact.issue==1 && fact.completion==2 && !fact.error && fact.native_serial && fact.completion_serial && fact.backend_queue,"scalar_native_completed");
  if(fact.semantic_kind==2)++body_facts;
  if(fact.semantic_kind==3){++copy_facts;copy_bytes+=fact.source_size;check(fact.source_backing_serial==backing.backing_serial && fact.source_offset==0 && fact.source_size==4,"scalar_exact_readback_source");}
  std::fprintf(stderr,"SYSTEM_SCALAR_FACT semantic=%u source=%llu offset=%llu bytes=%llu native=%llu completed=%llu queue=%llu\n",fact.semantic_kind,(unsigned long long)fact.source_backing_serial,(unsigned long long)fact.source_offset,(unsigned long long)fact.source_size,(unsigned long long)fact.native_serial,(unsigned long long)fact.completion_serial,(unsigned long long)fact.backend_queue);
 }
 check(body_facts==1 && copy_facts==1 && copy_bytes==4,"scalar_exact_native_work");
 require(scalar_copy(bridge,job.ticket,report.revision,&value,sizeof(value),&written).native_error,"scalar_copy");check(written==4 && value==SCALAR_EXPECTED_WORD,"scalar_exact_output");
 check(scalar_cancel(bridge,job.ticket).native_error==ERR_BUSY,"scalar_completed_result_immutable");
 bool released=false;
 for(uint32_t pass=0;pass<5000;++pass){
  auto result=scalar_release(bridge,job.ticket);if(result.native_error==OK){released=true;break;}
  check(result.native_error==ERR_BUSY,"scalar_release_only_pending");require(scalar_observe(bridge,job.ticket,&budget,&report).native_error,"scalar_set_retire_observe");std::this_thread::sleep_for(std::chrono::milliseconds(1));
 }
 check(released,"scalar_internal_set_physical_return");check(scalar_release(bridge,job.ticket).native_error!=OK,"scalar_ack_once");
 GDExtensionNativeUnitRetireRequestV1 uq{abi,unit.unit,3,0};GDExtensionNativeUnitRetireRefV1 uref{};GDExtensionNativeUnitRetireReportV1 ureport{};
 require(unit_retire(bridge,&uq,&uref,&ureport).native_error,"unit_retire");
 for(uint32_t pass=0;pass<5000 && !ureport.state;++pass){require(unit_observe(bridge,uref,&budget,&ureport).native_error,"unit_observe");if(!ureport.state)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(ureport.state==1 && ureport.freed_mask==3 && !ureport.unresolved_mask && ureport.shader_free_event_seq && ureport.pipeline_free_event_seq,"unit_actual_free");require(unit_release(bridge,uref).native_error,"unit_physical_ack");
 GDExtensionStorageRetireRequestV1 sq{abi,backing.backing,backing.rid,backing.backing_serial,3,0,41,2,0};GDExtensionStorageRetireRefV1 sref{};GDExtensionStorageRetireReportV1 sreport{};
 require(storage_retire(bridge,&sq,&sref,&sreport).native_error,"storage_retire");
 for(uint32_t pass=0;pass<5000 && !sreport.state;++pass){require(storage_observe(bridge,sref,&budget,&sreport).native_error,"storage_observe");if(!sreport.state)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 check(sreport.state==1 && sreport.return_status==1 && sreport.event_seq && !sreport.native_error,"storage_actual_free");require(storage_release(bridge,sref).native_error,"storage_physical_ack");
 require(close_bridge(bridge).native_error,"bridge_close");require(release_session(session).native_error,"session_release");require(release_context(c).native_error,"context_release");
 std::fprintf(stderr,"SYSTEM_SCALAR_COMPLETE exact_word=%08x bytes=%u native_complete=1 physical_free=1 ack=1\n",value,written);
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
 if (!load(get,"device_session_system_scalar_record_v1",scalar_record) ||
     !load(get,"device_session_system_scalar_observe_v1",scalar_observe) ||
     !load(get,"device_session_system_scalar_facts_v1",scalar_facts) ||
     !load(get,"device_session_system_scalar_copy_v1",scalar_copy) ||
     !load(get,"device_session_system_scalar_cancel_v1",scalar_cancel) ||
     !load(get,"device_session_system_scalar_release_v1",scalar_release) ||
     !load(get,"callable_custom_create2",make_callable) ||
     !load(get,"global_get_singleton",get_singleton)) { std::fprintf(stderr,"SYSTEM_SCALAR_PREFLIGHT_RED effects=0\n");std::_Exit(86); }

 if (!load(get,"device_session_qualification_rt_configure_v1",rt_configure) || !load(get,"device_session_qualification_rt_reserve_v1",rt_reserve) || !load(get,"device_session_qualification_rt_activate_v1",rt_activate) || !load(get,"device_session_qualification_rt_observe_v1",rt_observe) || !load(get,"device_session_qualification_rt_release_v1",rt_release)) {std::fprintf(stderr,"QRT_PREFLIGHT_RED effects=0\n");std::_Exit(86);}

 if (!load(get,"device_session_qualification_resource_retire_observe_v1",typed_observe) ||
!load(get,"device_session_qualification_resource_retire_release_v1",typed_release) ||
!load(get,"device_session_qualification_prepare_uploads_v2",prepare_ticket) ||
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
 registration.entry_classes = GDEXTENSION_DEVICE_EXIT_ENTRY_SCENE_READY_V1 | (uint64_t(1) << 8);
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
