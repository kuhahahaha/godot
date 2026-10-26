#include "tests/test_macros.h"
TEST_FORCE_LINK(test_qualification_metal_calls)
#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#include "drivers/metal/rendering_device_driver_metal3.h"
#include "drivers/metal/metal3_objects.h"
#include <thread>
class TestQualificationMetalCallAccess {
public:
 static Mutex &mutex(RenderingDeviceDriverMetal &driver){return driver.completion_records_mutex;}
 static void near_end(RenderingDeviceDriverMetal &driver){driver.next_native_call_serial=UINT64_MAX-1;}
 static Mutex &copy_mutex(RenderingDeviceDriverMetal &driver){return driver.copy_queue_mutex;}
 static void queue_copy(RenderingDeviceDriverMetal &driver,RenderingDeviceDriver::BufferID buffer,const uint8_t *bytes,uint32_t size) {
  auto *info=reinterpret_cast<RenderingDeviceDriverMetal::BufferInfo *>(buffer.id);
  driver._copy_queue_copy_to_buffer(Span<uint8_t>(bytes,size),info->buffer.get());
 }
 static uint64_t reserve(RenderingDeviceDriverMetal &driver,uint32_t count){return driver.reserve_native_call_serials(count);}
};

TEST_CASE("[QualificationNative][Metal] actual hidden and final commits have distinct retained identities") {
 using RDD=RenderingDeviceDriver;
 RenderingContextDriverMetal context;REQUIRE(context.initialize()==OK);
 MTL3::RenderingDeviceDriverMetal driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 auto queue=driver.command_queue_create(RDD::CommandQueueFamilyID(1));
 auto pool=driver.command_pool_create(RDD::CommandQueueFamilyID(1),RDD::COMMAND_BUFFER_TYPE_PRIMARY);
 auto command=driver.command_buffer_create(pool);auto fence=driver.fence_create();
 auto buffer=driver.buffer_create(4,RDD::BUFFER_USAGE_TRANSFER_TO_BIT,RDD::MEMORY_ALLOCATION_TYPE_CPU,0);
 REQUIRE(bool(buffer));REQUIRE(driver.command_buffer_begin(command));
 driver.command_clear_buffer(command,buffer,0,4);
 // Exercise the original wrapper's real pre-execute commit, as hidden native
 // command splits do. This is an actual Metal commit, not a fabricated fact.
 reinterpret_cast<MTL3::MDCommandBuffer *>(command.id)->commit();
 REQUIRE(driver.command_buffer_begin(command));driver.command_buffer_end(command);
 RDD::QueueCallReport report;
 REQUIRE(driver.command_queue_execute_and_present_observed(queue,{},command,{},fence,{},report)==OK);
 CHECK(report.calls.size()==2);
 HashSet<uint64_t> serials;
 for(const auto &call:report.calls) {
  CHECK(call.serial!=0);CHECK(call.completion_serial!=0);CHECK_FALSE(serials.has(call.serial));serials.insert(call.serial);
 }
 REQUIRE(driver.fence_wait_observed(fence,report.submission_serial).matches(report.submission_serial));
 for(const auto &call:report.calls) { CHECK(driver.native_call_poll(call.serial).matches(call.completion_serial));CHECK_FALSE(driver.native_call_poll(call.serial+100000).matches(call.completion_serial)); }

#ifdef THREADS_ENABLED
 auto &mutex=TestQualificationMetalCallAccess::mutex(driver);mutex.lock();RDD::FencePollResult contended;
 std::thread observer([&]{contended=driver.native_call_poll(report.calls[0].serial);});observer.join();
 CHECK_FALSE(contended.completed);mutex.unlock();CHECK(driver.native_call_poll(report.calls[0].serial).matches(report.calls[0].completion_serial));
#endif
 const uint64_t retained=report.calls[0].serial;
 REQUIRE(driver.native_call_retain(retained)==OK);
 REQUIRE(driver.fence_recycle(fence,report.submission_serial)==OK);
 for(const auto &call:report.calls) {
  if(call.serial==retained)CHECK(driver.native_call_poll(call.serial).matches(call.completion_serial));
  else CHECK_FALSE(driver.native_call_poll(call.serial).matches(call.completion_serial));
 }
 REQUIRE(driver.native_call_release(retained)==OK);CHECK_FALSE(driver.native_call_poll(retained).matches(retained));CHECK(driver.native_call_release(retained)!=OK);
 RDD::QueueCallReport empty;
 REQUIRE(driver.command_queue_execute_and_present_observed(queue,{},{},{},{},{},empty)==OK);
 CHECK_FALSE(empty.issued);CHECK(empty.calls.is_empty());
 driver.fence_free(fence);driver.buffer_free(buffer);driver.command_pool_free(pool);
 TestQualificationMetalCallAccess::near_end(driver);
 CHECK(TestQualificationMetalCallAccess::reserve(driver,3)==0);
 CHECK(TestQualificationMetalCallAccess::reserve(driver,2)==UINT64_MAX-1);
 CHECK(TestQualificationMetalCallAccess::reserve(driver,1)==0);
 CHECK(TestQualificationMetalCallAccess::reserve(driver,1)==0);
}
#endif

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
TEST_CASE("[QualificationNative][Metal] original hidden copy queue retains exact bytes and nonwaiting source") {
 using RDD=RenderingDeviceDriver;
 RenderingContextDriverMetal context;REQUIRE(context.initialize()==OK);
 MTL3::RenderingDeviceDriverMetal driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 auto buffer=driver.buffer_create(16,RDD::BUFFER_USAGE_TRANSFER_TO_BIT,RDD::MEMORY_ALLOCATION_TYPE_CPU,0);
 REQUIRE(bool(buffer));const uint8_t bytes[16]={3,1,4,1,5,9,2,6,5,3,5,8,9,7,9,3};
 TestQualificationMetalCallAccess::queue_copy(driver,buffer,bytes,16);
#ifdef THREADS_ENABLED
 auto &mutex=TestQualificationMetalCallAccess::copy_mutex(driver);mutex.lock();
 Error blocked=OK;RDD::QueueCallReport absent;
 std::thread contender([&]{blocked=driver.qualification_prepare_segment(absent);});contender.join();
 CHECK(blocked==ERR_BUSY);CHECK(absent.calls.is_empty());mutex.unlock();
#endif
 RDD::QueueCallReport issued;Error state=driver.qualification_prepare_segment(issued);
 REQUIRE(state==ERR_BUSY);REQUIRE(issued.calls.size()==1);const uint64_t serial=issued.calls[0].serial;REQUIRE(serial!=0);
 RDD::QueueCallReport retry;
 for(uint32_t i=0;i<100000 && state==ERR_BUSY;++i){state=driver.qualification_prepare_segment(retry);std::this_thread::yield();}
 REQUIRE(state==OK);CHECK(retry.calls.is_empty());CHECK(driver.native_call_poll(serial).matches(serial));
 uint8_t *mapped=driver.buffer_map(buffer);REQUIRE(mapped);CHECK(memcmp(mapped,bytes,16)==0);driver.buffer_unmap(buffer);
 // The copy queue relinquished its original owner, but its transferred source
 // observation keeps the callback cell alive until the Frame/consumer releases it.
 REQUIRE(driver.native_call_release(serial)==OK);CHECK_FALSE(driver.native_call_poll(serial).matches(serial));
 RDD::QueueCallReport no_work;CHECK(driver.qualification_prepare_segment(no_work)==OK);CHECK(no_work.calls.is_empty());
 driver.buffer_free(buffer);
}
#endif
