#include "tests/test_macros.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/thread.h"
#include <thread>
#include <atomic>
TEST_FORCE_LINK(test_qualification_ack)
class TestQualificationAckAccess {
public:
 static void interleaving(bool native) {
  using namespace GDExtensionDeviceExit;
  // Deliberately constructs the original Registry cells, not a second service.
  // This unit control covers ack arbitration only, not physical proof issuance.
  auto *registry=memnew(Registry);RenderingDevice device;auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=3;
  registry->device_records[0].object=&device;registry->device_records[0].live=true;registry->device_records[0].epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){} ~Holder()=default;};Holder holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=2;registry->qualification_bridges[0]=bridge;
  auto *storage=memnew(Registry::QualificationBridge::Storage);auto *unit=memnew(Registry::QualificationBridge::NativeUnit);
  bridge->storage[0]=storage;bridge->units[0]=unit;storage->serial=unit->serial=23;storage->retire_serial=unit->retire_serial=31;storage->queued=unit->queued=true;
  auto pending=native?registry->qualification_native_unit_retire_release({71,11},{71,31}):registry->qualification_storage_retire_release({71,11},{71,31});
  CHECK(pending.native_error==ERR_BUSY);CHECK(pending.reason==16);CHECK(bridge->children==2);CHECK(record.private_refs==3);
  std::atomic<bool> visible=false,ack_attempted=false;
  std::thread service([&]{
   {MutexLock lock(registry->mutex);storage->report.state=unit->report.state=1;storage->physical.free_event=37;unit->report.freed_mask=unit->backing.known_mask=3;}
   visible=true;while(!ack_attempted.load())std::this_thread::yield();
   {MutexLock lock(registry->mutex);storage->queued=unit->queued=false;--bridge->children;--record.private_refs;}
  });
  while(!visible.load())std::this_thread::yield();
  auto first=native?registry->qualification_native_unit_retire_release({71,11},{71,31}):registry->qualification_storage_retire_release({71,11},{71,31});
  CHECK(first.native_error==ERR_BUSY);CHECK(first.reason==19);CHECK(bridge->children==2);CHECK(record.private_refs==3);CHECK_FALSE((native?unit->acknowledged:storage->acknowledged));
  ack_attempted=true;service.join();
  auto second=native?registry->qualification_native_unit_retire_release({71,11},{71,31}):registry->qualification_storage_retire_release({71,11},{71,31});
  CHECK(second.native_error==OK);CHECK(bridge->children==0);CHECK(record.private_refs==1);CHECK((native?unit->acknowledged:storage->acknowledged));
  auto stale=native?registry->qualification_native_unit_retire_release({71,11},{71,31}):registry->qualification_storage_retire_release({71,11},{71,31});CHECK(stale.native_error!=OK);CHECK(stale.reason==17);CHECK(record.private_refs==1);
  RenderingDevice::singleton=previous;device.is_main_instance=false;registry->qualification_bridges[0]=nullptr;memdelete(storage);memdelete(unit);memdelete(bridge);memdelete(registry);
 }
};
#ifdef THREADS_ENABLED
TEST_CASE("[Qualification] Returned before queue tail keeps storage ack retryable and exact once") {TestQualificationAckAccess::interleaving(false);}
TEST_CASE("[Qualification] Returned before queue tail keeps native unit ack retryable and exact once") {TestQualificationAckAccess::interleaving(true);}
#endif
