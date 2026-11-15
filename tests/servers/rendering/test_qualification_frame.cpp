#include "tests/test_macros.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"

TEST_FORCE_LINK(test_qualification_frame)

class TestQualificationFrameAccess {
public:
 static void attach(RenderingDevice &device, RenderingDeviceDriver &driver) {
  device.driver=&driver;device.frames.resize(1);
  device.frames[0].fence=RenderingDeviceDriver::FenceID(13);
  device.frames[0].fence_signaled=true;
  RenderingDeviceDriver::QueueCallReport report;report.submission_serial=41;
  device.frames[0].chain_reports.push_back(report);
 }
 static Error poll(RenderingDevice &device) { return device._stall_for_frame(0,true); }
 static bool pending(const RenderingDevice &device) { return device.frames[0].fence_signaled; }
 static void detach(RenderingDevice &device) { device.frames.clear();device.driver=nullptr; }
};
namespace TestQualificationFrame {
class Driver : public TransferTestDriverBase {
public:
 FencePollResult completion;
 int polls=0,waits=0,recycles=0;
 Error fence_wait(FenceID) override { waits++; return OK; }
 FencePollResult fence_poll(FenceID fence,uint64_t serial) override {
  CHECK(fence==FenceID(13));CHECK(serial==41);polls++;return completion;
 }
 Error fence_recycle(FenceID fence,uint64_t serial) override {
  CHECK(fence==FenceID(13));CHECK(serial==41);recycles++;return OK;
 }
};
TEST_CASE("[RenderingDevice][Qualification] pending and wrong fence preserve frame without wait or recycle") {
 RenderingDevice device;Driver driver;
 TestQualificationFrameAccess::attach(device,driver);
 CHECK(TestQualificationFrameAccess::poll(device)==ERR_BUSY);
 CHECK(TestQualificationFrameAccess::pending(device));
 driver.completion.completed=true;driver.completion.completed_serial=42;
 CHECK(TestQualificationFrameAccess::poll(device)==ERR_BUSY);
 CHECK(TestQualificationFrameAccess::pending(device));
 CHECK(driver.polls==2);CHECK(driver.waits==0);CHECK(driver.recycles==0);
 TestQualificationFrameAccess::detach(device);
}
TEST_CASE("[RenderingDevice][Qualification] matching successful poll completes without a legacy fence wait") {
 RenderingDevice device;Driver driver;
 TestQualificationFrameAccess::attach(device,driver);
 driver.completion.completed=true;driver.completion.completed_serial=41;
 CHECK(TestQualificationFrameAccess::poll(device)==OK);
 CHECK_FALSE(TestQualificationFrameAccess::pending(device));
 CHECK(driver.polls==1);CHECK(driver.waits==0);CHECK(driver.recycles==1);
 TestQualificationFrameAccess::detach(device);
}
}
