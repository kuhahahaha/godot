#include "tests/test_macros.h"
#include <thread>
#include <atomic>
#include <chrono>
#include "core/os/thread.h"
#include "core/crypto/crypto_core.h"
#include "servers/rendering/rendering_device.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
TEST_FORCE_LINK(test_qualification_physical)
class TestQualificationPhysicalAccess {
public:
 static void ticket_reverse_unlink(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;device.render_thread_id=Thread::get_caller_id();
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=5;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=4;registry->qualification_bridges[0]=bridge;
  auto *storage=memnew(Registry::QualificationBridge::Storage);storage->serial=31;storage->physical.bridge=11;storage->physical.child=31;bridge->storage[0]=storage;
  RID rid=device._qualification_storage_create(128,&storage->physical);REQUIRE(rid.is_valid());
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  auto *unit=memnew(Registry::QualificationBridge::NativeUnit);unit->serial=32;unit->shader.bridge=11;unit->shader.child=32;bridge->units[0]=unit;
  auto *first=memnew(QualificationTicket);auto *other=memnew(QualificationTicket);first->serial=23;other->serial=24;first->bridge_serial=other->bridge_serial=11;first->physically_retired=true;other->physically_retired=false;
  bridge->tickets[0]=first;bridge->tickets[1]=other;first->physical_uses.push_back(&storage->physical);first->physical_uses.push_back(&unit->shader);other->physical_uses.push_back(&storage->physical);storage->physical.uses.push_back(first);storage->physical.uses.push_back(other);unit->shader.uses.push_back(first);
  const GDExtensionQualificationBridgeRefV1 ref{71,11};const GDExtensionQualificationTicketRefV1 ticket{71,23};
  first->accepted_rt_refs=1;CHECK(registry->qualification_release(ref,ticket).native_error==ERR_BUSY);first->accepted_rt_refs=0;
  first->frame_refs=1;CHECK(registry->qualification_release(ref,ticket).native_error==ERR_BUSY);first->frame_refs=0;
  first->queued=true;CHECK(registry->qualification_release(ref,ticket).native_error==ERR_BUSY);first->queued=false;
#ifdef THREADS_ENABLED
  std::atomic<bool> locked=false,done=false;std::thread worker([&]{device._thread_safe_.lock();locked=true;while(!done.load())std::this_thread::yield();device._thread_safe_.unlock();});
  while(!locked.load())std::this_thread::yield();
  auto blocked=registry->qualification_release(ref,ticket);CHECK(blocked.native_error==ERR_BUSY);CHECK(blocked.reason==19);
  CHECK_FALSE(first->acknowledged);CHECK(storage->physical.uses.size()==2);CHECK(first->physical_uses.size()==2);CHECK(unit->shader.uses.size()==1);
  done=true;worker.join();
#endif
  if(!first->acknowledged) {
   REQUIRE(registry->qualification_release(ref,ticket).native_error==OK);
   CHECK(first->physical_uses.is_empty());CHECK(storage->physical.uses.size()==1);CHECK(storage->physical.uses[0]==other);CHECK(unit->shader.uses.is_empty());
   CHECK(other->physical_uses.size()==1);CHECK(bridge->children==3);CHECK(record.private_refs==4);
   CHECK(registry->qualification_release(ref,ticket).reason==17);
   storage->retiring=true;storage->physical.use_closed=true;registry->qualification_retire_step(bridge,&device,31,1);
   CHECK(device.storage_buffer_owner.owns(rid));CHECK_FALSE(storage->physical.enqueued);CHECK(storage->report.reason==3);
   other->physically_retired=true;registry->qualification_retire_step(bridge,&device,31,1);CHECK_FALSE(device.storage_buffer_owner.owns(rid));CHECK(storage->physical.enqueued);
   device._free_pending_resources(0);CHECK(storage->physical.free_event!=0);
  }
  registry->qualification_bridges[0]=nullptr;RenderingDevice::singleton=previous;device.is_main_instance=false;memdelete(first);memdelete(other);memdelete(storage);memdelete(unit);memdelete(bridge);memdelete(registry);
 }
 static void foreign_bridge_pin_is_rejected(RenderingDevice &device) {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *bridge=memnew(Registry::QualificationBridge);auto *foreign=memnew(Registry::QualificationBridge);
  bridge->serial=11;foreign->serial=12;registry->qualification_bridges[0]=bridge;registry->qualification_bridges[1]=foreign;
  auto *storage=memnew(Registry::QualificationBridge::Storage);storage->serial=31;storage->physical.bridge=12;storage->physical.child=31;foreign->storage[0]=storage;
  RID rid=device._qualification_storage_create(128,&storage->physical);REQUIRE(rid.is_valid());
  QualificationTicket ticket;ticket.serial=23;ticket.bridge_serial=11;ticket.physically_retired=true;LocalVector<RID> resources;resources.push_back(rid);
  Error result;{MutexLock registry_lock(registry->mutex);MutexLock rd_lock(device._thread_safe_);result=device._qualification_pin_resources(&ticket,resources);}
  CHECK(result==ERR_UNAUTHORIZED);CHECK(ticket.resource_pins.is_empty());CHECK(ticket.physical_uses.is_empty());CHECK(storage->physical.uses.is_empty());CHECK(ticket.physically_retired.load());
  storage->physical.use_closed=true;device._free_internal(rid);device._free_pending_resources(0);
  registry->qualification_bridges[0]=registry->qualification_bridges[1]=nullptr;memdelete(storage);memdelete(foreign);memdelete(bridge);memdelete(registry);
 }
 static void ticket_empty_release_during_rd_contention(RenderingDevice &device) {
#ifdef THREADS_ENABLED
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=2;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=1;registry->qualification_bridges[0]=bridge;
  auto *ticket=memnew(QualificationTicket);ticket->serial=23;ticket->bridge_serial=11;ticket->physically_retired=true;bridge->tickets[0]=ticket;
  std::atomic<bool> locked=false,done=false;std::thread worker([&]{device._thread_safe_.lock();locked=true;while(!done.load())std::this_thread::yield();device._thread_safe_.unlock();});
  while(!locked.load())std::this_thread::yield();const auto result=registry->qualification_release({71,11},{71,23});
  CHECK(result.native_error==OK);CHECK(ticket->acknowledged);CHECK(bridge->children==0);done=true;worker.join();
  registry->qualification_bridges[0]=nullptr;RenderingDevice::singleton=previous;device.is_main_instance=false;memdelete(ticket);memdelete(bridge);memdelete(registry);
#endif
 }
 static void ticket_relock_busy(RenderingDevice &device,RenderingDeviceDriver &driver,std::atomic<bool> &start,std::atomic<bool> &locked) {
#ifdef THREADS_ENABLED
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;device.driver=&driver;
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=3;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=2;registry->qualification_bridges[0]=bridge;
  auto *storage=memnew(Registry::QualificationBridge::Storage);storage->serial=31;storage->physical.bridge=11;storage->physical.child=31;bridge->storage[0]=storage;
  auto *ticket=memnew(QualificationTicket);ticket->serial=23;ticket->bridge_serial=11;ticket->physically_retired=true;bridge->tickets[0]=ticket;
  ticket->physical_uses.push_back(&storage->physical);storage->physical.uses.push_back(ticket);
  RenderingDeviceDriver::NativeCallFact source;source.serial=101;ticket->natives.push_back({source,0,0,0,0,true,false});
  std::atomic<bool> done=false;std::thread worker([&]{while(!start.load() && !done.load())std::this_thread::yield();if(done.load())return;device._thread_safe_.lock();locked=true;while(!done.load())std::this_thread::yield();device._thread_safe_.unlock();});
  const auto blocked=registry->qualification_release({71,11},{71,23});CHECK(blocked.native_error==ERR_BUSY);CHECK(blocked.reason==19);
  CHECK(ticket->natives[0].owner_released);CHECK_FALSE(ticket->acknowledged);CHECK(ticket->physical_uses.size()==1);CHECK(storage->physical.uses.size()==1);
  done=true;worker.join();
  if(!ticket->acknowledged){CHECK(registry->qualification_release({71,11},{71,23}).native_error==OK);CHECK(ticket->physical_uses.is_empty());CHECK(storage->physical.uses.is_empty());}
  registry->qualification_bridges[0]=nullptr;RenderingDevice::singleton=previous;device.driver=nullptr;device.is_main_instance=false;memdelete(ticket);memdelete(storage);memdelete(bridge);memdelete(registry);
#endif
 }
 static void ticket_native_release_retry(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;device.driver=&driver;
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=3;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=2;registry->qualification_bridges[0]=bridge;
  auto *storage=memnew(Registry::QualificationBridge::Storage);storage->serial=31;storage->physical.bridge=11;storage->physical.child=31;bridge->storage[0]=storage;
  auto *ticket=memnew(QualificationTicket);ticket->serial=23;ticket->bridge_serial=11;ticket->physically_retired=true;bridge->tickets[0]=ticket;
  ticket->physical_uses.push_back(&storage->physical);storage->physical.uses.push_back(ticket);
  RenderingDeviceDriver::NativeCallFact a,b;a.serial=101;b.serial=102;ticket->natives.push_back({a,0,0,0,0,true,false});ticket->natives.push_back({b,0,0,0,0,true,false});
  auto first=registry->qualification_release({71,11},{71,23});CHECK(first.native_error==ERR_BUSY);CHECK(first.reason==19);
  CHECK(ticket->natives[0].owner_released);CHECK_FALSE(ticket->natives[1].owner_released);CHECK_FALSE(ticket->acknowledged);CHECK(ticket->physical_uses.size()==1);CHECK(storage->physical.uses.size()==1);
  REQUIRE(registry->qualification_release({71,11},{71,23}).native_error==OK);CHECK(ticket->natives[1].owner_released);CHECK(ticket->physical_uses.is_empty());CHECK(storage->physical.uses.is_empty());
  registry->qualification_bridges[0]=nullptr;RenderingDevice::singleton=previous;device.driver=nullptr;device.is_main_instance=false;memdelete(ticket);memdelete(storage);memdelete(bridge);memdelete(registry);
 }
 static void ticket_bad_reverse_edges(RenderingDevice &device,uint32_t mode) {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=3;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=2;registry->qualification_bridges[0]=bridge;
  auto *storage=memnew(Registry::QualificationBridge::Storage);storage->serial=31;storage->physical.bridge=11;storage->physical.child=31;bridge->storage[0]=storage;
  auto *ticket=memnew(QualificationTicket);ticket->serial=23;ticket->bridge_serial=11;ticket->physically_retired=true;bridge->tickets[0]=ticket;
  ticket->physical_uses.push_back(&storage->physical);storage->physical.uses.push_back(ticket);
  if(mode==0)ticket->physical_uses.push_back(&storage->physical);
  if(mode==1)storage->physical.uses.push_back(ticket);
  if(mode==2)storage->physical.uses.clear();
  if(mode==3)ticket->physical_uses.push_back(reinterpret_cast<QualificationPhysicalReturn *>(uintptr_t(1)));
  if(mode==4)ticket->physical_uses.clear();
  const auto result=registry->qualification_release({71,11},{71,23});CHECK(result.native_error==ERR_INVALID_DATA);CHECK_FALSE(ticket->acknowledged);CHECK(bridge->children==2);CHECK(record.private_refs==3);
  CHECK(ticket->physical_uses.size()==(mode==0 || mode==3?2:mode==4?0:1));CHECK(storage->physical.uses.size()==(mode==1?2:mode==2?0:1));
  registry->qualification_bridges[0]=nullptr;RenderingDevice::singleton=previous;device.is_main_instance=false;memdelete(ticket);memdelete(storage);memdelete(bridge);memdelete(registry);
 }
 static void retired_prepared_upload(RenderingDevice &device, RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  // Original cells are fixture-enrolled; calls below exercise the original
  // API lifecycle. This CPU control grants no production/native authority.
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;
  RenderingDevice::singleton=&device;device.is_main_instance=true;device.render_thread_id=Thread::get_caller_id();
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.guard_enrollment=GuardEnrollment::QUALIFIED;record.private_refs=1;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;d.execution_thread=Thread::get_caller_id();d.refs=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;holder.qualification_children=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;registry->qualification_bridges[0]=bridge;
  const GDExtensionQualificationBridgeRefV1 ref{71,11};
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  GDExtensionStorageCreateRequestV1 create{{1,16,0},128,3,1,41,1,0};GDExtensionStorageBackingV1 backing={};
  REQUIRE(registry->qualification_storage_create(ref,&create,&backing).native_error==OK);
  uint8_t bytes[32]={};GDExtensionQualificationUploadPlanV1 plan={};plan.abi={1,16,0};plan.role=7;plan.owner_kind=GDEXTENSION_QUALIFICATION_UPLOAD_OWNER_E05_V1;plan.rid=backing.rid;plan.backing_serial=backing.backing_serial;plan.byte_size=sizeof(bytes);plan.e05_backing=backing.backing;
  REQUIRE(CryptoCore::sha256(bytes,sizeof(bytes),plan.payload_digest)==OK);
  GDExtensionQualificationUploadPrepareV1 prepare={};prepare.abi={1,16,0};prepare.record_generation=37;prepare.request_nonce=39;prepare.library_generation=1;prepare.qualification_session_serial=7;prepare.parent_serial=31;prepare.reservation_serial=97;prepare.attempt=prepare.upload_count=prepare.max_segments=1;prepare.expected_result_bytes=16;
  GDExtensionQualificationTicketRefV1 ticket_ref={};GDExtensionQualificationReportV1 report={};
  REQUIRE(registry->qualification_prepare_uploads(ref,&prepare,&plan,1,&ticket_ref,&report).native_error==OK);
  auto *ticket=bridge->tickets[0];REQUIRE(ticket!=nullptr);LocalVector<RID> pins;pins.push_back(RID::from_uint64(backing.rid));
  REQUIRE(device._qualification_pin_resources(ticket,pins)==OK);CHECK(ticket->physical_uses.size()==1);
  registry->qualification_ticket_publish(ticket,&device);CHECK(ticket->physically_retired.load());CHECK(ticket->resource_pins.is_empty());CHECK_FALSE(ticket->admission_closed.load());
  GDExtensionQualificationUploadRequestV1 upload={};upload.abi={1,16,0};upload.record_generation=37;upload.parent_serial=31;upload.rid=plan.rid;upload.backing_serial=plan.backing_serial;upload.byte_size=sizeof(bytes);upload.role=plan.role;upload.owner_kind=plan.owner_kind;upload.e05_backing=plan.e05_backing;upload.expected_revision=ticket->report.revision;memcpy(upload.payload_digest,plan.payload_digest,32);
  GDExtensionQualificationRtConfigV1 config{{1,40,0},1,312,200,1,256};REQUIRE(registry->qualification_rt_configure(ref,&config).native_error==OK);
  alignas(8) uint8_t payload[48]={};memcpy(payload,&ticket_ref,16);memcpy(payload+16,bytes,sizeof(bytes));
  GDExtensionQualificationRtRequestV1 header{{1,56,0},8,0,1,sizeof(payload),sizeof(upload),sizeof(GDExtensionQualificationUploadReportV1),1,0};GDExtensionQualificationRtRefV1 rt={};
  REQUIRE(registry->qualification_rt_reserve(ref,&header,&upload,payload,&rt).native_error==OK);CHECK(ticket->accepted_rt_refs==1);
  REQUIRE(registry->qualification_upload_destination(bridge,&device,plan)==OK);
  GDExtensionStorageRetireRequestV1 retire{{1,16,0},backing.backing,backing.rid,backing.backing_serial,3,1,41,2,0};GDExtensionStorageRetireRefV1 retired={};GDExtensionStorageRetireReportV1 retired_report={};
  REQUIRE(registry->qualification_storage_retire(ref,&retire,&retired,&retired_report).native_error==OK);
  CHECK_FALSE(device.storage_buffer_owner.owns(RID::from_uint64(backing.rid)));
  GDExtensionQualificationUploadSegmentV1 segment={};GDExtensionQualificationUploadReportV1 upload_report={};
  auto refused=registry->qualification_upload(ref,ticket_ref,&upload,bytes,sizeof(bytes),&segment,1,&upload_report);
  CHECK(refused.native_error==ERR_INVALID_DATA);CHECK(refused.reason==16);CHECK(upload_report.revision==0);CHECK(ticket->graphs.is_empty());CHECK(ticket->natives.is_empty());CHECK(device.draw_graph.qualification_command_count()==0);
  CHECK(registry->qualification_release(ref,ticket_ref).native_error==ERR_BUSY);
  REQUIRE(registry->qualification_rt_release(ref,rt).native_error==OK);CHECK(ticket->accepted_rt_refs==0);
  REQUIRE(registry->qualification_release(ref,ticket_ref).native_error==OK);CHECK(registry->qualification_release(ref,ticket_ref).reason==17);
  device._free_pending_resources(0);
  // Drive the original synchronous maintenance step on this fixture Registry;
  // the global RenderingServer scheduler belongs to the global Registry.
  registry->qualification_retire_step(bridge,&device,bridge->storage[0]->serial,1);
  const GDExtensionQualificationBudgetV1 budget{{1,16,0},1,1000,0};
  REQUIRE(registry->qualification_storage_retire_observe(ref,retired,&budget,&retired_report).native_error==OK);CHECK(retired_report.state==1);
  REQUIRE(registry->qualification_storage_retire_release(ref,retired).native_error==OK);CHECK(bridge->children==0);CHECK(record.private_refs==1);
  REQUIRE(registry->qualification_close(ref).native_error==OK);CHECK(registry->qualification_bridges[0]==nullptr);CHECK(holder.qualification_children==0);
  RenderingDevice::singleton=previous;device.is_main_instance=false;memdelete(registry);
 }
 static void retire_window(RenderingDevice &device,RenderingDeviceDriver &driver,uint32_t mode) {
  using namespace GDExtensionDeviceExit;
  auto *bridge=memnew(Registry::QualificationBridge);auto *child=memnew(Registry::QualificationBridge::Storage);
  bridge->storage[0]=child;child->serial=31;child->retiring=true;child->physical.bridge=11;child->physical.child=31;
  RID rid=device._qualification_storage_create(128,&child->physical);REQUIRE(rid.is_valid());child->physical.use_closed=true;
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  if(mode==1)device.draw_list.active=true;
  if(mode==2)device.compute_list.active=true;
  if(mode==3)device.raytracing_list.active=true;
  if(mode==4)device.draw_graph.add_driver_callback(nullptr,nullptr,{},{});
  Registry::get_singleton().qualification_retire_step(bridge,&device,31,1);
  CHECK(device.storage_buffer_owner.owns(rid));CHECK_FALSE(child->physical.enqueued);CHECK(child->report.state==0);
  device.draw_list.active=false;device.compute_list.active=false;device.raytracing_list.active=false;device.draw_graph.begin();
  Registry::get_singleton().qualification_retire_step(bridge,&device,31,1);
  CHECK_FALSE(device.storage_buffer_owner.owns(rid));CHECK(child->physical.enqueued);CHECK(child->physical.free_event==0);
  // This control checks enqueue safety only. Keep the queued return cell alive
  // through the original driver's disposal, then delete the original child.
  device._free_pending_resources(0);CHECK(child->physical.free_event!=0);memdelete(child);memdelete(bridge);
 }
 static void retire_after_service_error(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *bridge=memnew(Registry::QualificationBridge);auto *child=memnew(Registry::QualificationBridge::Storage);
  bridge->storage[0]=child;child->serial=31;child->retiring=true;child->physical.bridge=11;child->physical.child=31;
  RID rid=device._qualification_storage_create(128,&child->physical);REQUIRE(rid.is_valid());child->physical.use_closed=true;
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  auto &registry=Registry::get_singleton();registry.qualification_retire_step(bridge,&device,31,1);
  REQUIRE(child->physical.enqueued);REQUIRE(child->physical.free_event==0);
  // Model the original service's published non-BUSY maintenance error after
  // enqueue; the actual buffer remains owned by the original dispose queue.
  child->maintenance_error=ERR_CANT_ACQUIRE_RESOURCE;child->report.state=2;child->report.reason=5;child->report.native_error=ERR_CANT_ACQUIRE_RESOURCE;
  registry.qualification_retire_step(bridge,&device,31,1);
  CHECK(child->report.state==2);CHECK(child->report.return_status==0);CHECK(child->report.reason==5);CHECK(child->report.native_error==ERR_CANT_ACQUIRE_RESOURCE);
  device._free_pending_resources(0);REQUIRE(child->physical.free_event!=0);
  registry.qualification_retire_step(bridge,&device,31,1);
  CHECK(child->report.event_seq==child->physical.free_event);CHECK(child->report.state==2);CHECK(child->report.return_status==0);CHECK(child->report.reason==5);CHECK(child->report.native_error==ERR_CANT_ACQUIRE_RESOURCE);
  memdelete(child);memdelete(bridge);
 }
 static void unknown_does_not_enqueue(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *bridge=memnew(Registry::QualificationBridge);auto *child=memnew(Registry::QualificationBridge::Storage);
  bridge->storage[0]=child;child->serial=31;child->retiring=true;child->physical.bridge=11;child->physical.child=31;child->physical.use_closed=true;
  RID rid=device._qualification_storage_create(128,&child->physical);REQUIRE(rid.is_valid());
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  child->maintenance_error=FAILED;child->report.state=2;child->report.reason=5;child->report.native_error=FAILED;
  Registry::get_singleton().qualification_retire_step(bridge,&device,31,1);
  CHECK(device.storage_buffer_owner.owns(rid));CHECK_FALSE(child->physical.enqueued);CHECK(child->report.state==2);CHECK(child->report.reason==5);
  // Explicit fixture cleanup follows the assertion and is not an owner ack.
  if(device.storage_buffer_owner.owns(rid))device._free_internal(rid);
  device._free_pending_resources(0);memdelete(child);memdelete(bridge);
 }
 static void service_poll_error(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;
  device.is_main_instance=true;device.render_thread_id=Thread::get_caller_id();registry->instance=71;
  auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.guard_enrollment=GuardEnrollment::QUALIFIED;record.private_refs=3;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;d.execution_thread=Thread::get_caller_id();d.refs=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=2;registry->qualification_bridges[0]=bridge;
  auto *child=memnew(Registry::QualificationBridge::Storage);bridge->storage[0]=child;child->serial=31;child->retire_serial=33;child->retiring=child->queued=true;child->physical.bridge=11;child->physical.child=31;child->physical.use_closed=true;
  RID rid=device._qualification_storage_create(128,&child->physical);REQUIRE(rid.is_valid());
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  RenderingDeviceDriver::QueueCallReport chain;RenderingDeviceDriver::NativeCallFact call;call.serial=call.completion_serial=51;chain.calls.push_back(call);device.frames[0].chain_reports.push_back(chain);
  registry->qualification_service(11,31,1,1,5000);
  CHECK_FALSE(child->queued);CHECK(child->physical.enqueued);CHECK(child->report.state==2);CHECK(child->report.reason==5);CHECK(child->report.native_error==FAILED);CHECK(device.frame_wait_native_code.load()==-21);
  CHECK(registry->qualification_storage_retire_release({71,11},{71,33}).native_error==ERR_BUSY);
  registry->qualification_retire_step(bridge,&device,31,1);CHECK(child->report.state==2);CHECK(child->report.reason==5);
  device._free_pending_resources(0);REQUIRE(child->physical.free_event!=0);registry->qualification_retire_step(bridge,&device,31,1);
  CHECK(child->report.state==2);CHECK(child->report.return_status==0);CHECK(child->report.native_error==FAILED);CHECK(registry->qualification_storage_retire_release({71,11},{71,33}).native_error==ERR_BUSY);
  device.frames[0].chain_reports.clear();registry->qualification_bridges[0]=nullptr;RenderingDevice::singleton=previous;device.is_main_instance=false;memdelete(child);memdelete(bridge);memdelete(registry);
 }
 static void service_error_before_shader_enqueue(RenderingDevice &device,RenderingDeviceDriver &driver,bool earlier_error,bool saturated,bool close_before) {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;
  device.is_main_instance=true;device.render_thread_id=Thread::get_caller_id();registry->instance=71;
  auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.guard_enrollment=GuardEnrollment::QUALIFIED;record.private_refs=3;
  auto &d=registry->device_records[0];d.object=&device;d.live=true;d.epoch=1;d.execution_thread=Thread::get_caller_id();d.refs=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){}~Holder()=default;} holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=2;registry->qualification_bridges[0]=bridge;
  auto *child=memnew(Registry::QualificationBridge::NativeUnit);bridge->units[0]=child;child->serial=31;child->retire_serial=33;child->retiring=child->queued=true;child->backing.known_mask=1;child->report.report_revision=saturated?UINT64_MAX:1;
  if(earlier_error){child->maintenance_error=ERR_CANT_ACQUIRE_RESOURCE;child->report.state=2;child->report.reason=5;child->report.native_error=ERR_CANT_ACQUIRE_RESOURCE;}
  child->shader.bridge=11;child->shader.child=31;child->shader.serial=43;child->shader.use_closed=true;
  RenderingDevice::Shader shader;shader.driver_id=RenderingDeviceDriver::ShaderID(19);shader.qualification_return=&child->shader;
  RID rid=device.shader_owner.make_rid(shader);child->shader.rid=rid.get_id();
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  RenderingDeviceDriver::QueueCallReport chain;RenderingDeviceDriver::NativeCallFact call;call.serial=call.completion_serial=51;chain.calls.push_back(call);device.frames[0].chain_reports.push_back(chain);
  const auto original=child->report;
  if(close_before){CHECK(registry->qualification_close({71,11}).native_error==ERR_BUSY);CHECK(child->queued);CHECK(bridge->children==2);}
  registry->qualification_service(11,31,2,1,5000);
  CHECK(device.frame_wait_native_code.load()==-21);CHECK(child->maintenance_error==(earlier_error?ERR_CANT_ACQUIRE_RESOURCE:FAILED));
  if(saturated){CHECK(memcmp(&original,&child->report,sizeof(original))==0);}else{CHECK(child->report.state==2);CHECK(child->report.native_error==(earlier_error?ERR_CANT_ACQUIRE_RESOURCE:FAILED));}
  CHECK(device.shader_owner.owns(rid));CHECK_FALSE(child->shader.enqueued);CHECK(child->report.return_status==0);CHECK(registry->qualification_native_unit_retire_release({71,11},{71,33}).native_error==ERR_BUSY);CHECK(registry->qualification_close({71,11}).native_error==ERR_BUSY);
  // The test fixture performs explicit cleanup after observing the service result.
  if(device.shader_owner.owns(rid))device._free_internal(rid);
  device._free_pending_resources(0);device.frames[0].chain_reports.clear();registry->qualification_bridges[0]=nullptr;RenderingDevice::singleton=previous;device.is_main_instance=false;memdelete(child);memdelete(bridge);memdelete(registry);
 }
 static void native_revision_exhausted(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *bridge=memnew(Registry::QualificationBridge);auto *child=memnew(Registry::QualificationBridge::NativeUnit);
  bridge->units[0]=child;child->serial=31;child->retiring=true;child->backing.known_mask=1;child->report.report_revision=UINT64_MAX;child->report.reason=1;
  child->shader.bridge=11;child->shader.child=31;child->shader.serial=43;child->shader.use_closed=true;
  RenderingDevice::Shader shader;shader.driver_id=RenderingDeviceDriver::ShaderID(19);shader.qualification_return=&child->shader;
  RID rid=device.shader_owner.make_rid(shader);child->shader.rid=rid.get_id();
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  const auto original=child->report;Registry::get_singleton().qualification_retire_step(bridge,&device,31,2);
  CHECK(device.shader_owner.owns(rid));CHECK_FALSE(child->shader.enqueued);CHECK(memcmp(&original,&child->report,sizeof(original))==0);
  if(device.shader_owner.owns(rid))device._free_internal(rid);
  device._free_pending_resources(0);memdelete(child);memdelete(bridge);
 }
 static void native_nonerror_unknown_can_settle(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *bridge=memnew(Registry::QualificationBridge);auto *child=memnew(Registry::QualificationBridge::NativeUnit);
  bridge->units[0]=child;child->serial=31;child->retiring=true;child->backing.known_mask=1;
  child->shader.rid=41;child->shader.serial=43;child->shader.enqueued=true;child->shader.frame_slot=0;child->shader.frame_epoch=71;child->shader.free_event=47;
  child->report.report_revision=1;child->report.state=2;child->report.reason=5;
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  Registry::get_singleton().qualification_retire_step(bridge,&device,31,2);
  CHECK(child->report.state==1);CHECK(child->report.return_status==1);CHECK(child->report.native_error==OK);CHECK(child->report.shader_free_event_seq==47);CHECK(child->report.report_revision==2);
  memdelete(child);memdelete(bridge);
 }
 static void native_retire_after_service_error(RenderingDevice &device,RenderingDeviceDriver &driver) {
  using namespace GDExtensionDeviceExit;
  auto *bridge=memnew(Registry::QualificationBridge);auto *child=memnew(Registry::QualificationBridge::NativeUnit);
  bridge->units[0]=child;child->serial=31;child->retiring=true;child->backing.known_mask=1;
  child->shader.rid=41;child->shader.serial=43;child->shader.enqueued=true;child->shader.frame_slot=0;child->shader.frame_epoch=71;
  child->report.report_revision=1;child->maintenance_error=ERR_CANT_ACQUIRE_RESOURCE;child->report.state=2;child->report.reason=5;child->report.native_error=ERR_CANT_ACQUIRE_RESOURCE;
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  auto &registry=Registry::get_singleton();registry.qualification_retire_step(bridge,&device,31,2);
  CHECK(child->report.state==2);CHECK(child->report.return_status==0);CHECK(child->report.reason==5);CHECK(child->report.native_error==ERR_CANT_ACQUIRE_RESOURCE);
  // Isolated report reducer control: actual driver disposal is covered by
  // the storage fixture above, not asserted by this synthetic event.
  child->shader.free_event=47;registry.qualification_retire_step(bridge,&device,31,2);
  CHECK(child->report.freed_mask==1);CHECK(child->report.shader_free_event_seq==47);CHECK(child->report.state==2);CHECK(child->report.return_status==0);CHECK(child->report.reason==5);CHECK(child->report.native_error==ERR_CANT_ACQUIRE_RESOURCE);
  memdelete(child);memdelete(bridge);
 }
 static void attach(RenderingDevice &device, RenderingDeviceDriver &driver) { device.driver=&driver;device.frames.resize(1);device.frames[0].qualification_epoch=71;device.qualification_retiring=true; }
 static RID create(RenderingDevice &device,GDExtensionDeviceExit::QualificationPhysicalReturn &cell) { cell.bridge=11;cell.child=13;return device._qualification_storage_create(128,&cell); }
 static bool marked(RenderingDevice &device,RID rid,GDExtensionDeviceExit::QualificationPhysicalReturn &cell) { auto *b=device.storage_buffer_owner.get_or_null(rid);return b && b->qualification_return==&cell && cell.serial==b->backing_serial && cell.rid==rid.get_id(); }
 static void enqueue(RenderingDevice &device,RID rid,GDExtensionDeviceExit::QualificationPhysicalReturn &cell) { cell.use_closed=true;device._free_internal(rid); }
 static uint64_t event(const GDExtensionDeviceExit::QualificationPhysicalReturn &cell) { return cell.free_event; }
 static void dispose(RenderingDevice &device) { device._free_pending_resources(0); }
 static RID dependent(RenderingDevice &device,GDExtensionDeviceExit::QualificationPhysicalReturn &cell) {
  RenderingDevice::UniformSet set;set.driver_id=RenderingDeviceDriver::UniformSetID(23);set.qualification_dependencies.push_back(&cell);cell.dependent_children.fetch_add(1);return device.uniform_set_owner.make_rid(set);
 }
 static uint32_t dependents(const GDExtensionDeviceExit::QualificationPhysicalReturn &cell) {return cell.dependent_children.load();}
 static void reserve_unused_workers(RenderingDevice &device) {device.transfer_worker_pool.resize(4);}
 static Error worker_try(RenderingDevice &device) {return device._submit_transfer_workers({},true);}
 static BinaryMutex &worker_mutex(RenderingDevice &device) {return device.transfer_worker_pool_mutex;}
 static void detach(RenderingDevice &device) { device.frames.clear();device.driver=nullptr; }
};
namespace TestQualificationPhysical {
class Driver : public TransferTestDriverBase {
public:
 GDExtensionDeviceExit::QualificationPhysicalReturn *cell=nullptr;
 int creates=0,frees=0,sets_freed=0;
 bool fail_poll=false;bool fail_native_once=false;std::atomic<bool> *release_start=nullptr,*release_locked=nullptr;int polls=0;int shaders_freed=0;int release_one=0,release_two=0;
 Error native_call_release(uint64_t serial) override {if(serial==101){++release_one;if(release_start && release_locked){release_start->store(true);const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!release_locked->load() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();if(!release_locked->load())return ERR_BUSY;}return OK;}if(serial==102){++release_two;if(fail_native_once && release_two==1)return ERR_BUSY;return OK;}return ERR_DOES_NOT_EXIST;}
 bool native_call_observation_supported() const override {return true;}
 FencePollResult native_call_poll(uint64_t serial) override { FencePollResult result;if(fail_poll){++polls;result.completed_serial=serial;result.native_error=true;result.native_code=-21;}return result;}
 bool shader_retire_ready_nonblocking(ShaderID) const override {return polls>0;}
 void shader_free(ShaderID id) override {CHECK(id==ShaderID(19));++shaders_freed;}
 uint32_t qualification_native_call_bound(uint32_t slots) const override {return slots;}
 void uniform_set_free(UniformSetID id) override {CHECK(id==UniformSetID(23));CHECK(TestQualificationPhysicalAccess::dependents(*cell)==1);sets_freed++;}
 BufferID buffer_create(uint64_t size,BitField<BufferUsageBits>,MemoryAllocationType,uint64_t) override { CHECK(size==128);creates++;return BufferID(19); }
 void buffer_free(BufferID id) override { CHECK(id==BufferID(19));if(cell)CHECK(TestQualificationPhysicalAccess::event(*cell)==0);frees++; }
};
TEST_CASE("[RenderingDevice][Qualification] buffer return follows exact physical free and survives RID erase") {
 RenderingDevice device;Driver driver;GDExtensionDeviceExit::QualificationPhysicalReturn cell;driver.cell=&cell;
 TestQualificationPhysicalAccess::attach(device,driver);
 RID rid=TestQualificationPhysicalAccess::create(device,cell);
 CHECK(rid.is_valid());CHECK(TestQualificationPhysicalAccess::marked(device,rid,cell));
 TestQualificationPhysicalAccess::enqueue(device,rid,cell);
 CHECK_FALSE(TestQualificationPhysicalAccess::marked(device,rid,cell));CHECK(TestQualificationPhysicalAccess::event(cell)==0);CHECK(driver.frees==0);
 TestQualificationPhysicalAccess::dispose(device);
 CHECK(driver.frees==1);const auto event=TestQualificationPhysicalAccess::event(cell);CHECK(event!=0);
 TestQualificationPhysicalAccess::dispose(device);CHECK(driver.frees==1);CHECK(TestQualificationPhysicalAccess::event(cell)==event);
 TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[RenderingDevice][Qualification] dependent descriptor pin survives RID erase until driver return") {
 RenderingDevice device;Driver driver;GDExtensionDeviceExit::QualificationPhysicalReturn cell;driver.cell=&cell;
 TestQualificationPhysicalAccess::attach(device,driver);
 RID set=TestQualificationPhysicalAccess::dependent(device,cell);
 device.free_rid(set);CHECK(TestQualificationPhysicalAccess::dependents(cell)==1);CHECK(driver.sets_freed==0);
 TestQualificationPhysicalAccess::dispose(device);CHECK(driver.sets_freed==1);CHECK(TestQualificationPhysicalAccess::dependents(cell)==0);
 TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[Qualification] retire keeps exact original RID live while lists or callbacks are active") {
 for(uint32_t mode=1;mode<=4;++mode) {
  RenderingDevice device;Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
  TestQualificationPhysicalAccess::retire_window(device,driver,mode);CHECK(driver.frees==1);TestQualificationPhysicalAccess::detach(device);
 }
}
#ifdef THREADS_ENABLED
TEST_CASE("[RenderingDevice][Qualification] contended transfer lock refuses maintenance before driver effect") {
 RenderingDevice device;Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 auto &mutex=TestQualificationPhysicalAccess::worker_mutex(device);mutex.lock();
 Error result=OK;std::thread worker([&]{result=TestQualificationPhysicalAccess::worker_try(device);});worker.join();
 CHECK(result==ERR_BUSY);mutex.unlock();TestQualificationPhysicalAccess::reserve_unused_workers(device);CHECK(TestQualificationPhysicalAccess::worker_try(device)==OK);
 TestQualificationPhysicalAccess::detach(device);
}
#endif

}

TEST_CASE("[Qualification] retired E05 backing rejects same prepared upload and preserves RT release and bridge teardown") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::retired_prepared_upload(device,driver);CHECK(driver.creates==1);CHECK(driver.frees==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] storage retire retains unknown after maintenance failure and physical free") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::retire_after_service_error(device,driver);CHECK(driver.creates==1);CHECK(driver.frees==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] native unit retire reducer retains maintenance unknown on observed event") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::native_retire_after_service_error(device,driver);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] service native poll error remains unknown after storage free") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.fail_poll=true;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::service_poll_error(device,driver);CHECK(driver.creates==1);CHECK(driver.frees==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] storage maintenance unknown prevents new retire enqueue") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::unknown_does_not_enqueue(device,driver);CHECK(driver.creates==1);CHECK(driver.frees==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] service poll error precedes new native shader retirement") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.fail_poll=true;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device,driver,false,false,false);CHECK(driver.polls==1);CHECK(driver.shaders_freed==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] service preserves first native retire maintenance error") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.fail_poll=true;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device,driver,true,false,false);CHECK(driver.polls==1);CHECK(driver.shaders_freed==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] native unit exhausted retire revision freezes report and prevents new enqueue") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.polls=1;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::native_revision_exhausted(device,driver);CHECK(driver.shaders_freed==1);TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[Qualification] native unit unknown without maintenance error accepts original later event") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::native_nonerror_unknown_can_settle(device,driver);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] exhausted native report keeps service error private and close busy") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.fail_poll=true;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device,driver,false,true,false);CHECK(driver.polls==1);CHECK(driver.shaders_freed==1);TestQualificationPhysicalAccess::detach(device);
}
TEST_CASE("[Qualification] queued native service retains maintenance error across close") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.fail_poll=true;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device,driver,false,false,true);CHECK(driver.polls==1);CHECK(driver.shaders_freed==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] queued native close retains preexisting maintenance error") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.fail_poll=true;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::service_error_before_shader_enqueue(device,driver,true,false,true);CHECK(driver.polls==1);CHECK(driver.shaders_freed==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] ticket ack removes only its original reverse uses after RD contention") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::ticket_reverse_unlink(device,driver);CHECK(driver.creates==1);CHECK(driver.frees==1);TestQualificationPhysicalAccess::detach(device);
}

TEST_CASE("[Qualification] ticket ack rejects damaged or duplicate reverse edges without partial unlink") {
 for(uint32_t mode=0;mode<5;++mode){RenderingDevice device;TestQualificationPhysicalAccess::ticket_bad_reverse_edges(device,mode);}
}

TEST_CASE("[Qualification] ticket native release retry retains reverse edges until all observers return") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;driver.fail_native_once=true;
 TestQualificationPhysicalAccess::ticket_native_release_retry(device,driver);CHECK(driver.release_one==1);CHECK(driver.release_two==2);
}

#ifdef THREADS_ENABLED
TEST_CASE("[Qualification] empty ticket ack preserves progress during RD contention") {
 RenderingDevice device;TestQualificationPhysicalAccess::ticket_empty_release_during_rd_contention(device);
}
TEST_CASE("[Qualification] ticket observer return survives later RD contention without replay") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;std::atomic<bool> start=false,locked=false;driver.release_start=&start;driver.release_locked=&locked;
 TestQualificationPhysicalAccess::ticket_relock_busy(device,driver,start,locked);CHECK(driver.release_one==1);
}
#endif

TEST_CASE("[Qualification] foreign bridge physical child cannot acquire ticket reverse use") {
 RenderingDevice device;TestQualificationPhysical::Driver driver;TestQualificationPhysicalAccess::attach(device,driver);
 TestQualificationPhysicalAccess::foreign_bridge_pin_is_rejected(device);CHECK(driver.creates==1);CHECK(driver.frees==1);TestQualificationPhysicalAccess::detach(device);
}
