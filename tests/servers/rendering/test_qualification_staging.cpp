#include "tests/test_macros.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
TEST_FORCE_LINK(test_qualification_staging)
class TestQualificationStagingAccess {
public:
 static RID attach(RenderingDevice &device, RenderingDeviceDriver &driver) {
  device.driver=&driver;device.frames.resize(2);device.frames_drawn=10;device.qualification_recording=true;
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},2,RenderingDeviceDriver::CommandQueueFamilyID(1),0);
  device.draw_graph.begin();
  auto &staging=device.download_staging_buffers;staging.blocks.resize(1);staging.block_size=staging.max_size=64;
  staging.blocks.write[0].driver_id=RenderingDeviceDriver::BufferID(51);staging.blocks.write[0].frame_used=10;staging.blocks.write[0].fill_amount=32;
  RenderingDevice::Buffer buffer;buffer.driver_id=RenderingDeviceDriver::BufferID(71);buffer.size=64;buffer.backing_serial=11;buffer.draw_tracker=RenderingDeviceGraph::resource_tracker_create();buffer.draw_tracker->buffer_driver_id=buffer.driver_id;
  return device.storage_buffer_owner.make_rid(buffer);
 }
 static auto progress(){RenderingDevice::QualificationUploadProgress progress;progress.max_segments=8;return progress;}
 static void upload_fixture(RenderingDevice &device,uint8_t *bytes){device.upload_staging_buffers=device.download_staging_buffers;device.upload_staging_buffers.blocks.write[0].data_ptr=bytes;}
 static Error upload(RenderingDevice &device,RID rid,const uint8_t *bytes,RenderingDevice::QualificationUploadProgress &progress){return device._buffer_update(device.storage_buffer_owner.get_or_null(rid),rid,0,64,bytes,&progress);}
 static void upload_in_flight(RenderingDevice &device){device.upload_staging_buffers.blocks.write[0].frame_used=9;device.upload_staging_buffers.blocks.write[0].fill_amount=64;}
 static void upload_reusable(RenderingDevice &device){device.upload_staging_buffers.blocks.write[0].frame_used=0;}
 static auto result(){Ref<RenderingDevice::BufferGetDataResult> result;result.instantiate();result->bytes.resize(64);result->qualification_owned=true;return result;}
 static Error append(RenderingDevice &device,RID rid,const Ref<RenderingDevice::BufferGetDataResult> &result){return device._buffer_get_data_async(rid,Callable(),0,64,result);}
 static uint32_t queued(RenderingDevice &device){return device.frames[0].download_buffer_get_data_requests.size();}
 static void in_flight(RenderingDevice &device){device.download_staging_buffers.blocks.write[0].frame_used=9;device.download_staging_buffers.blocks.write[0].fill_amount=64;}
 static void reusable(RenderingDevice &device){device.download_staging_buffers.blocks.write[0].frame_used=0;}
 static void detach(RenderingDevice &device,RID rid){auto *b=device.storage_buffer_owner.get_or_null(rid);RenderingDeviceGraph::resource_tracker_free(b->draw_tracker);device.storage_buffer_owner.free(rid);device.frames.clear();device.download_staging_buffers.blocks.clear();device.upload_staging_buffers.blocks.clear();device.driver=nullptr;}
};
namespace TestQualificationStaging {
class Driver:public TransferTestDriverBase { public:int waits=0;Error fence_wait(FenceID)override{++waits;return ERR_TIMEOUT;} };
TEST_CASE("[Qualification] readback retains actual graph prefix without either staging wait") {
 RenderingDevice device;Driver driver;RID rid=TestQualificationStagingAccess::attach(device,driver);auto result=TestQualificationStagingAccess::result();
 CHECK(TestQualificationStagingAccess::append(device,rid,result)==ERR_BUSY);
 REQUIRE(result->copies.size()==1);CHECK(result->copies[0].handle.is_valid());CHECK(result->copies[0].region.src_offset==0);CHECK(result->copies[0].region.size==32);
 CHECK(result->issued_size==32);CHECK(result->completed_size==0);CHECK_FALSE(result->delivered);CHECK(result->error==OK);CHECK(TestQualificationStagingAccess::queued(device)==1);CHECK(driver.waits==0);
 TestQualificationStagingAccess::in_flight(device);
 CHECK(TestQualificationStagingAccess::append(device,rid,result)==ERR_BUSY);CHECK(result->copies.size()==1);CHECK(result->issued_size==32);CHECK(driver.waits==0);
 TestQualificationStagingAccess::reusable(device);
 CHECK(TestQualificationStagingAccess::append(device,rid,result)==OK);REQUIRE(result->copies.size()==2);CHECK(result->copies[1].region.src_offset==32);CHECK(result->copies[1].region.size==32);CHECK(result->issued_size==64);CHECK(driver.waits==0);
 TestQualificationStagingAccess::detach(device,rid);
}
TEST_CASE("[Qualification] in flight staging refuses before first readback append") {
 RenderingDevice device;Driver driver;RID rid=TestQualificationStagingAccess::attach(device,driver);auto result=TestQualificationStagingAccess::result();TestQualificationStagingAccess::in_flight(device);
 CHECK(TestQualificationStagingAccess::append(device,rid,result)==ERR_BUSY);CHECK(result->copies.is_empty());CHECK(result->issued_size==0);CHECK(TestQualificationStagingAccess::queued(device)==0);CHECK(driver.waits==0);
 TestQualificationStagingAccess::detach(device,rid);
}
TEST_CASE("[Qualification] checked upload resumes only actual suffix through both exhausted staging cases") {
 RenderingDevice device;Driver driver;RID rid=TestQualificationStagingAccess::attach(device,driver);
 uint8_t staging[64]={},bytes[64];for(uint32_t i=0;i<64;++i)bytes[i]=i+1;
 TestQualificationStagingAccess::upload_fixture(device,staging);auto progress=TestQualificationStagingAccess::progress();
 CHECK(TestQualificationStagingAccess::upload(device,rid,bytes,progress)==ERR_BUSY);
 REQUIRE(progress.segments.size()==1);CHECK(progress.segments[0].handle.is_valid());CHECK(progress.segments[0].begin==0);CHECK(progress.segments[0].end==32);CHECK(progress.consumed==32);CHECK(memcmp(staging+32,bytes,32)==0);CHECK(driver.waits==0);
 TestQualificationStagingAccess::upload_in_flight(device);
 CHECK(TestQualificationStagingAccess::upload(device,rid,bytes,progress)==ERR_BUSY);CHECK(progress.segments.size()==1);CHECK(progress.consumed==32);CHECK(driver.waits==0);
 TestQualificationStagingAccess::upload_reusable(device);
 CHECK(TestQualificationStagingAccess::upload(device,rid,bytes,progress)==OK);REQUIRE(progress.segments.size()==2);CHECK(progress.segments[1].begin==32);CHECK(progress.segments[1].end==64);CHECK(progress.consumed==64);CHECK(memcmp(staging,bytes+32,32)==0);CHECK(driver.waits==0);
 CHECK(TestQualificationStagingAccess::upload(device,rid,bytes,progress)==OK);CHECK(progress.segments.size()==2);
 TestQualificationStagingAccess::detach(device,rid);
}

}
