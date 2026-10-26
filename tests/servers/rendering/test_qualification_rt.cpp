#include "tests/test_macros.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/thread.h"
#include <thread>
TEST_FORCE_LINK(test_qualification_rt)

// Registry transport/arbitration controls. These construct original cells and
// a metadata-only Buffer fixture; they do not issue production authority or
// certify a GPU result. Native Ready/RT controls are separate.
class TestQualificationRtAccess {
 using Registry=GDExtensionDeviceExit::Registry;
 using Ticket=GDExtensionDeviceExit::QualificationTicket;
 struct Fixture {
  Registry *registry=memnew(Registry);
  RenderingDevice device;
  RenderingDevice *previous=RenderingDevice::singleton;
  struct Holder:GDExtensionDeviceExit::HolderRetirement{Holder():HolderRetirement(GDExtensionDeviceExit::RecordRef{}){}~Holder()=default;} holder;
  Registry::QualificationBridge *bridge=memnew(Registry::QualificationBridge);
  RID buffer;
  GDExtensionQualificationBridgeRefV1 ref{71,11};
  Fixture() {
   RenderingDevice::singleton=&device;device.is_main_instance=true;device.render_thread_id=Thread::get_caller_id();
   registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.guard_enrollment=GDExtensionDeviceExit::GuardEnrollment::QUALIFIED;record.private_refs=registry->private_limit-1;
   auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;d.execution_thread=Thread::get_caller_id();
   holder.generation=1;bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;registry->qualification_bridges[0]=bridge;
   RenderingDevice::Buffer b;b.size=64;b.backing_serial=17;buffer=device.storage_buffer_owner.make_rid(b);
  }
  void dispatch() {
   // The test returns one independent capacity pin, then accounts exactly the
   // same queue pin that the real bounded scalar enqueue reserves.
   --registry->records[0].private_refs;bridge->rt->queued=true;++bridge->children;++registry->records[0].private_refs;
   registry->qualification_rt_dispatch(bridge->serial,bridge->rt->serial);
  }
  ~Fixture(){RenderingDevice::singleton=previous;device.is_main_instance=false;device.storage_buffer_owner.free(buffer);registry->qualification_bridges[0]=nullptr;for(auto *ticket:bridge->tickets)if(ticket)memdelete(ticket);for(auto *child:bridge->storage)if(child)memdelete(child);for(auto *child:bridge->units)if(child)memdelete(child);if(bridge->rt)memdelete(bridge->rt);memdelete(bridge);memdelete(registry);}
 };
 static GDExtensionQualificationRtConfigV1 config(){return {{1,40,0},1,312,200,2,256};}
 static GDExtensionQualificationRtRequestV1 query(uint64_t operation){return {{1,56,0},1,0,operation,0,8,48,0,0};}
public:
 static void cleanup(bool malformed, bool duplicate) {
  for(uint32_t kind:{3u,5u}) {
   Fixture f;auto c=config();REQUIRE(f.registry->qualification_rt_configure(f.ref,&c).native_error==OK);
   f.registry->records[0].private_refs=4;
   auto *storage=memnew(Registry::QualificationBridge::Storage);storage->serial=29;storage->creating=false;storage->backing.rid=f.buffer.get_id();storage->backing.backing_serial=17;storage->origin.allocation_slot=3;storage->origin.part=1;storage->origin.allocation_generation=41;f.bridge->storage[0]=storage;
   auto *unit=memnew(Registry::QualificationBridge::NativeUnit);unit->serial=31;unit->creating=false;f.bridge->units[0]=unit;
   GDExtensionStorageRetireRequestV1 sr{{1,16,0},{71,29},f.buffer.get_id(),17,3,1,41,2,0};
   GDExtensionNativeUnitRetireRequestV1 nr{{1,16,0},{71,31},43,0};
   GDExtensionQualificationRtRequestV1 h{{1,56,0},kind,0,1,0,kind==3?80u:48u,kind==3?136u:184u,0,0};
   f.registry->records[0].quiescing=true;
   const auto counter=f.registry->qualification_serials.next;const auto refs=f.registry->records[0].private_refs;const auto children=f.bridge->children;
   auto reserve=[&](const void *q){GDExtensionQualificationRtRefV1 out{};auto result=f.registry->qualification_rt_reserve(f.ref,&h,q,nullptr,&out);CHECK(out.handle_serial==0);CHECK_FALSE(f.bridge->rt->occupied);CHECK(f.registry->qualification_serials.next==counter);CHECK(f.registry->records[0].private_refs==refs);CHECK(f.bridge->children==children);return result;};
   if(malformed) {
    if(kind==3){
     for(int variant=0;variant<12;++variant){auto q=sr;switch(variant){case 0:q.backing.registry_instance++;break;case 1:q.backing.handle_serial++;break;case 2:q.rid++;break;case 3:q.observed_backing_serial++;break;case 4:q.allocation_slot++;break;case 5:q.part++;break;case 6:q.allocation_generation++;break;case 7:storage->creating=true;break;case 8:storage->acknowledged=true;break;case 9:storage->retiring=true;storage->retirement=sr;storage->retirement.retire_attempt++;break;case 10:f.bridge->storage[0]=nullptr;break;case 11:q.retire_attempt=0;break;}
      CHECK(reserve(&q).native_error!=OK);storage->creating=storage->acknowledged=storage->retiring=false;f.bridge->storage[0]=storage;}
    }else{
     for(int variant=0;variant<7;++variant){auto q=nr;switch(variant){case 0:q.unit.registry_instance++;break;case 1:q.unit.handle_serial++;break;case 2:unit->creating=true;break;case 3:unit->acknowledged=true;break;case 4:unit->retiring=true;unit->report.retire_operation=44;break;case 5:f.bridge->units[0]=nullptr;break;case 6:q.retire_operation=0;break;}
      CHECK(reserve(&q).native_error!=OK);unit->creating=unit->acknowledged=unit->retiring=false;f.bridge->units[0]=unit;}
    }
    continue;
   }
   GDExtensionQualificationRtRefV1 accepted{},same{};const void *request=kind==3?static_cast<const void*>(&sr):static_cast<const void*>(&nr);
   REQUIRE(f.registry->qualification_rt_reserve(f.ref,&h,request,nullptr,&accepted).native_error==OK);
   CHECK(accepted.handle_serial!=0);CHECK(f.bridge->children==children+1);CHECK(f.registry->records[0].private_refs==refs+1);CHECK_FALSE(storage->retiring);CHECK_FALSE(unit->retiring);
   if(duplicate){
    REQUIRE(f.registry->qualification_rt_reserve(f.ref,&h,request,nullptr,&same).native_error==OK);CHECK(same.handle_serial==accepted.handle_serial);CHECK(f.bridge->children==children+1);CHECK(f.registry->records[0].private_refs==refs+1);
    auto other=h;other.operation_serial++;same={};CHECK(f.registry->qualification_rt_reserve(f.ref,&other,request,nullptr,&same).native_error==ERR_BUSY);CHECK(same.handle_serial==0);
    auto changed_sr=sr;changed_sr.retire_attempt++;auto changed_nr=nr;changed_nr.retire_operation++;same={};CHECK(f.registry->qualification_rt_reserve(f.ref,&h,kind==3?static_cast<const void*>(&changed_sr):static_cast<const void*>(&changed_nr),nullptr,&same).native_error==ERR_INVALID_DATA);CHECK(same.handle_serial==0);
   }
   REQUIRE(f.registry->qualification_rt_release(f.ref,accepted).native_error==OK);CHECK(f.bridge->children==children);CHECK(f.registry->records[0].private_refs==refs);
  }
 }
 static void other_kinds_closed() {
  Fixture f;auto c=config();REQUIRE(f.registry->qualification_rt_configure(f.ref,&c).native_error==OK);f.registry->records[0].quiescing=true;
  constexpr uint32_t sizes[]={0,8,56,80,104,48,312,152,136,312},results[]={0,48,72,136,96,184,200,200,120,184};
  alignas(8) uint8_t request[312]={},payload[256]={};const auto refs=f.registry->records[0].private_refs;
  auto *ticket=memnew(Ticket);ticket->serial=23;ticket->bridge_serial=11;ticket->prepared=true;ticket->physically_retired=true;ticket->report.revision=5;ticket->preparation.record_generation=37;ticket->preparation.parent_serial=31;ticket->preparation.request_nonce=39;ticket->preparation.library_generation=1;ticket->preparation.qualification_session_serial=7;ticket->preparation.attempt=1;ticket->preparation.expected_result_bytes=16;ticket->uploads.resize(1);f.bridge->tickets[0]=ticket;
  auto &plan=ticket->uploads[0].plan;plan.abi={1,16,0};plan.role=7;plan.owner_kind=2;plan.rid=f.buffer.get_id();plan.backing_serial=17;plan.byte_size=32;ticket->uploads[0].progress.max_segments=2;
  for(uint32_t kind:{1u,2u,4u,6u,7u,8u,9u}){
   const auto serial=f.registry->qualification_serials.next;
   memset(request,0,sizeof(request));memset(payload,0,sizeof(payload));GDExtensionQualificationRtRequestV1 h{{1,56,0},kind,0,kind+1,0,sizes[kind],results[kind],kind==8?2u:0u,0};
   if(kind==1){auto rid=f.buffer.get_id();memcpy(request,&rid,8);}
   if(kind==2){GDExtensionStorageCreateRequestV1 q{{1,16,0},64,3,0,41,1,0};memcpy(request,&q,sizeof(q));}
   if(kind==4){GDExtensionNativeUnitCreateRequestV1 q{};q.abi={1,16,0};q.library_generation=1;q.preparation_generation=21;q.operation_serial=1;q.container_byte_count=4;memcpy(request,&q,sizeof(q));h.payload_bytes=4;}
   if(kind==6 || kind==9){GDExtensionQualificationRecordV1 q{};q.abi={1,16,0};q.record_generation=37;q.request_nonce=39;q.library_generation=1;q.qualification_session_serial=7;q.attempt=1;q.range_end=q.sample_count=4;q.body_groups_x=1;q.readback_size=16;memcpy(request,&q,sizeof(q));if(kind==9){ticket->uploads[0].attempted=true;ticket->uploads[0].progress.consumed=plan.byte_size;ticket->uploads[0].report.state=1;GDExtensionQualificationTicketRefV1 ref{71,23};memcpy(payload,&ref,sizeof(ref));h.payload_bytes=16;}}
   if(kind==7){GDExtensionQualificationUploadPrepareV1 q{};q.abi={1,16,0};q.record_generation=37;q.request_nonce=39;q.library_generation=1;q.qualification_session_serial=7;q.parent_serial=31;q.reservation_serial=97;q.attempt=q.upload_count=1;q.max_segments=2;q.expected_result_bytes=16;memcpy(request,&q,sizeof(q));memcpy(payload,&plan,sizeof(plan));h.payload_bytes=sizeof(plan);}
   if(kind==8){GDExtensionQualificationUploadRequestV1 q{};q.abi={1,16,0};q.record_generation=37;q.parent_serial=31;q.rid=plan.rid;q.backing_serial=17;q.byte_size=32;q.role=7;q.owner_kind=2;q.expected_revision=5;memcpy(request,&q,sizeof(q));GDExtensionQualificationTicketRefV1 ref{71,23};memcpy(payload,&ref,sizeof(ref));h.payload_bytes=48;}
   GDExtensionQualificationRtRefV1 out{};CHECK(f.registry->qualification_rt_reserve(f.ref,&h,request,h.payload_bytes?payload:nullptr,&out).native_error==ERR_BUSY);CHECK(out.handle_serial==0);CHECK_FALSE(f.bridge->rt->occupied);CHECK(f.bridge->children==0);CHECK(f.registry->records[0].private_refs==refs);CHECK(f.registry->qualification_serials.next==serial);CHECK(ticket->accepted_rt_refs==0);
   // The identical original POD/ticket must be reservable before quiescing;
   // an unrelated shape/identity refusal cannot make the closed gate pass.
   f.registry->records[0].quiescing=false;GDExtensionQualificationRtRefV1 admitted{};
   REQUIRE(f.registry->qualification_rt_reserve(f.ref,&h,request,h.payload_bytes?payload:nullptr,&admitted).native_error==OK);
   REQUIRE(admitted.handle_serial!=0);REQUIRE(f.registry->qualification_rt_release(f.ref,admitted).native_error==OK);f.registry->records[0].quiescing=true;
  }
 }
 static void one_accepted_call() {
  Fixture f;auto c=config();REQUIRE(f.registry->qualification_rt_configure(f.ref,&c).native_error==OK);
  auto header=query(1);uint64_t rid=f.buffer.get_id();GDExtensionQualificationRtRefV1 ref={};REQUIRE(f.registry->qualification_rt_reserve(f.ref,&header,&rid,nullptr,&ref).native_error==OK);
  auto *cell=f.bridge->rt;cell->active=cell->queued=cell->executing=true;cell->state=1;f.bridge->closing=true;
  auto *previous=Registry::active_qualification_bridge;const auto previous_serial=Registry::active_qualification_serial;Registry::active_qualification_bridge=f.bridge;Registry::active_qualification_serial=cell->serial;
  GDExtensionQualificationBackingV1 out={};CHECK(f.registry->qualification_backing(f.ref,rid,&out).native_error==OK);CHECK(out.backing_serial==17);
  const auto second=f.registry->qualification_backing(f.ref,rid,&out);CHECK(second.native_error==ERR_BUSY);CHECK(second.reason==16);CHECK(out.rid==0);
  Registry::active_qualification_bridge=previous;Registry::active_qualification_serial=previous_serial;cell->active=cell->queued=cell->executing=false;
 }
 static void carrier() {
  Fixture f;auto c=config();auto wrong=c;wrong.abi.struct_size=16;CHECK(f.registry->qualification_rt_configure(f.ref,&wrong).native_error==ERR_INVALID_PARAMETER);CHECK(f.bridge->rt==nullptr);
  wrong=c;wrong.max_payload_bytes=67108865;CHECK(f.registry->qualification_rt_configure(f.ref,&wrong).native_error==ERR_INVALID_PARAMETER);CHECK(f.bridge->rt==nullptr);
  REQUIRE(f.registry->qualification_rt_configure(f.ref,&c).native_error==OK);
#ifdef THREADS_ENABLED
  GDExtensionDeviceSessionResultV1 foreign={};std::thread other([&]{foreign=f.registry->qualification_rt_configure(f.ref,&c);});other.join();CHECK(foreign.native_error==ERR_UNAUTHORIZED);CHECK(foreign.reason==15);
#endif
  auto header=query(1);uint64_t rid=f.buffer.get_id();GDExtensionQualificationRtRefV1 first={},repeat={};
  REQUIRE(f.registry->qualification_rt_reserve(f.ref,&header,&rid,nullptr,&first).native_error==OK);CHECK(first.handle_serial!=0);CHECK(f.bridge->children==1);CHECK(f.registry->records[0].private_refs==f.registry->private_limit);
  REQUIRE(f.registry->qualification_rt_reserve(f.ref,&header,&rid,nullptr,&repeat).native_error==OK);CHECK(repeat.handle_serial==first.handle_serial);CHECK(f.bridge->children==1);
  uint64_t changed=rid+1;repeat={};CHECK(f.registry->qualification_rt_reserve(f.ref,&header,&changed,nullptr,&repeat).native_error!=OK);CHECK(repeat.handle_serial==0);
  rid=0; // The accepted request is the original owned copy.
  REQUIRE(f.registry->qualification_rt_activate(f.ref,first).native_error==OK);CHECK_FALSE(f.bridge->rt->queued);CHECK(f.bridge->rt->state==1);CHECK(f.registry->qualification_rt_activate(f.ref,first).native_error==ERR_BUSY);
  GDExtensionQualificationRtObservationV1 observation={};GDExtensionQualificationBackingV1 backing={};
  REQUIRE(f.registry->qualification_rt_observe(f.ref,first,&observation,&backing,sizeof(backing),nullptr,0).native_error==OK);CHECK(observation.state==1);CHECK(observation.result_bytes==0);CHECK(backing.rid==0);CHECK(f.registry->qualification_rt_release(f.ref,first).native_error==ERR_BUSY);
  CHECK(f.registry->qualification_close(f.ref).native_error==ERR_BUSY);CHECK(f.bridge->closing);
  CHECK(f.registry->qualification_backing(f.ref,f.buffer.get_id(),&backing).native_error==ERR_BUSY);
  f.dispatch();CHECK(f.bridge->rt->state==2);CHECK_FALSE(f.bridge->rt->queued);CHECK_FALSE(f.bridge->rt->executing);CHECK(f.bridge->children==1);
  CHECK(f.registry->qualification_rt_release(f.ref,first).native_error==ERR_BUSY);
  alignas(8) uint8_t oversized[200];memset(oversized,0xa5,sizeof(oversized));
  CHECK(f.registry->qualification_rt_observe(f.ref,first,&observation,oversized,sizeof(oversized),nullptr,0).native_error==ERR_INVALID_PARAMETER);CHECK(observation.state==0);CHECK(oversized[0]==0);CHECK(oversized[199]==0);CHECK_FALSE(f.bridge->rt->observed);
  REQUIRE(f.registry->qualification_rt_observe(f.ref,first,&observation,&backing,sizeof(backing),nullptr,0).native_error==OK);CHECK(observation.state==2);CHECK(observation.call_result.native_error==OK);CHECK(observation.result_bytes==48);CHECK(backing.rid==f.buffer.get_id());CHECK(backing.backing_serial==17);CHECK(backing.byte_size==64);
  CHECK(f.registry->qualification_backing(f.ref,f.buffer.get_id(),&backing).native_error==ERR_BUSY); // private accepted-call scope ended.
  REQUIRE(f.registry->qualification_rt_release(f.ref,first).native_error==OK);CHECK(f.bridge->children==0);CHECK(f.registry->qualification_rt_release(f.ref,first).reason==17);
  f.bridge->closing=false;rid=f.buffer.get_id();repeat={};CHECK(f.registry->qualification_rt_reserve(f.ref,&header,&rid,nullptr,&repeat).reason==17);CHECK(f.registry->qualification_rt_configure(f.ref,&c).native_error==ERR_BUSY);
  header.operation_serial=2;REQUIRE(f.registry->qualification_rt_reserve(f.ref,&header,&rid,nullptr,&repeat).native_error==OK);CHECK(repeat.handle_serial!=first.handle_serial);CHECK(f.registry->qualification_rt_release(f.ref,repeat).native_error==OK);CHECK(f.bridge->children==0);
 }
 static void same_ticket_pin(bool cancel) {
  Fixture f;auto c=config();REQUIRE(f.registry->qualification_rt_configure(f.ref,&c).native_error==OK);
  auto *ticket=memnew(Ticket);ticket->serial=23;ticket->bridge_serial=11;ticket->prepared=true;ticket->physically_retired=true;ticket->report.revision=5;ticket->preparation.record_generation=37;ticket->preparation.parent_serial=31;ticket->uploads.resize(1);f.bridge->tickets[0]=ticket;++f.bridge->children;
  auto &upload=ticket->uploads[0];auto &plan=upload.plan;plan.abi={1,16,0};plan.role=7;plan.owner_kind=2;plan.rid=f.buffer.get_id();plan.backing_serial=17;plan.byte_size=64;upload.progress.max_segments=2;
  GDExtensionQualificationUploadRequestV1 request={};request.abi={1,16,0};request.record_generation=37;request.parent_serial=31;request.rid=plan.rid;request.backing_serial=17;request.byte_size=64;request.role=7;request.owner_kind=2;request.expected_revision=5;
  alignas(8) uint8_t payload[80]={};const GDExtensionQualificationTicketRefV1 child{71,23};memcpy(payload,&child,sizeof(child));payload[16]=9;
  GDExtensionQualificationRtRequestV1 header{{1,56,0},8,0,1,80,136,120,2,0};GDExtensionQualificationRtRefV1 ref={},same={};
  auto malformed=header;malformed.payload_bytes=64;CHECK(f.registry->qualification_rt_reserve(f.ref,&malformed,&request,payload,&ref).native_error!=OK);CHECK(ref.handle_serial==0);CHECK(ticket->accepted_rt_refs==0);CHECK_FALSE(f.bridge->rt->occupied);
  malformed=header;malformed.payload_bytes=UINT64_MAX;CHECK(f.registry->qualification_rt_reserve(f.ref,&malformed,&request,payload,&ref).native_error!=OK);CHECK(ref.handle_serial==0);CHECK(ticket->accepted_rt_refs==0);
  REQUIRE(f.registry->qualification_rt_reserve(f.ref,&header,&request,payload,&ref).native_error==OK);CHECK(ticket->accepted_rt_refs==1);CHECK(f.registry->qualification_release(f.ref,child).native_error==ERR_BUSY);
  REQUIRE(f.registry->qualification_rt_reserve(f.ref,&header,&request,payload,&same).native_error==OK);CHECK(same.handle_serial==ref.handle_serial);CHECK(ticket->accepted_rt_refs==1);
  payload[8]++;same={};CHECK(f.registry->qualification_rt_reserve(f.ref,&header,&request,payload,&same).native_error!=OK);CHECK(ticket->accepted_rt_refs==1);payload[8]--;
  payload[16]=99;CHECK(f.bridge->rt->payload[16]==9);
  if(cancel) {
   REQUIRE(f.registry->qualification_cancel(f.ref,child).native_error==OK);CHECK(ticket->stop_requested);CHECK(ticket->accepted_rt_refs==1);
   REQUIRE(f.registry->qualification_rt_activate(f.ref,ref).native_error==OK);CHECK_FALSE(f.bridge->rt->queued);
   f.dispatch();GDExtensionQualificationRtObservationV1 observation={};GDExtensionQualificationUploadReportV1 returned={};GDExtensionQualificationUploadSegmentV1 segments[2]={};
   REQUIRE(f.registry->qualification_rt_observe(f.ref,ref,&observation,&returned,sizeof(returned),segments,2).native_error==OK);CHECK(observation.state==2);CHECK(observation.call_result.native_error==ERR_BUSY);CHECK(observation.call_result.reason==16);CHECK(returned.revision==0);CHECK(observation.segment_count==0);CHECK(ticket->graphs.is_empty());CHECK(ticket->accepted_rt_refs==1);
  }
  REQUIRE(f.registry->qualification_rt_release(f.ref,ref).native_error==OK);CHECK(ticket->accepted_rt_refs==0);CHECK(f.bridge->children==1);
  if(cancel)f.registry->qualification_ticket_publish(ticket,&f.device);
  CHECK(f.registry->qualification_release(f.ref,child).native_error==OK);CHECK(f.bridge->children==0);CHECK(f.registry->qualification_rt_release(f.ref,ref).reason==17);
 }
};
TEST_CASE("[QualificationRt] fixed copied slot retries once across pressure close return and owner ack") {TestQualificationRtAccess::carrier();SUBCASE("cleanup_duplicate_and_possible_free"){TestQualificationRtAccess::cleanup(false,true);}}
TEST_CASE("[QualificationRt] preactivation abort drops exactly the accepted original ticket pin") {TestQualificationRtAccess::same_ticket_pin(false);}
TEST_CASE("[QualificationRt] accepted typed upload retains same ticket across owner cancel and returned refusal") {TestQualificationRtAccess::same_ticket_pin(true);}

TEST_CASE("[QualificationRt] accepted close scope admits its typed call exactly once") {TestQualificationRtAccess::one_accepted_call();SUBCASE("cleanup_kind3_kind5_after_quiesce"){TestQualificationRtAccess::cleanup(false,false);}SUBCASE("all_other_kinds_remain_closed"){TestQualificationRtAccess::other_kinds_closed();}SUBCASE("cleanup_child_auth_complete_request"){TestQualificationRtAccess::cleanup(true,false);}}
