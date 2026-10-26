#include "tests/test_macros.h"
TEST_FORCE_LINK(test_qualification_vulkan_allocation)
#if defined(VULKAN_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#include "drivers/vulkan/rendering_device_driver_vulkan.h"
class TestQualificationVulkanAllocationAccess {
public:
 static void fail(RenderingDeviceDriverVulkan &driver,uint32_t stage){driver.test_buffer_fault_stage=stage;}
 static uint32_t rollback(RenderingDeviceDriverVulkan &driver){return driver.test_buffer_rollback_mask;}
 static uint32_t allocations(RenderingDeviceDriverVulkan &driver){VmaTotalStatistics stats;vmaCalculateStatistics(driver.allocator,&stats);return stats.total.statistics.allocationCount;}
 static void clean_red_prefix(RenderingDeviceDriverVulkan &driver) {
  if(driver.test_buffer_prefix && !(driver.test_buffer_rollback_mask&1))vkDestroyBuffer(driver.vk_device,driver.test_buffer_prefix,RenderingContextDriverVulkan::get_allocation_callbacks(VK_OBJECT_TYPE_BUFFER));
  if(driver.test_buffer_allocation_prefix && !(driver.test_buffer_rollback_mask&2))vmaFreeMemory(driver.allocator,driver.test_buffer_allocation_prefix);
  driver.test_buffer_prefix=VK_NULL_HANDLE;driver.test_buffer_allocation_prefix=nullptr;driver.test_buffer_fault_stage=0;
 }
};
TEST_CASE("[QualificationNative][Vulkan] failed manual buffer creation returns its actual native prefix") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 for(uint32_t stage=1;stage<=2;++stage) {
  const uint32_t before=TestQualificationVulkanAllocationAccess::allocations(driver);TestQualificationVulkanAllocationAccess::fail(driver,stage);
  ERR_PRINT_OFF;
  auto buffer=driver.buffer_create(1048576,RenderingDeviceDriver::BUFFER_USAGE_STORAGE_BIT,RenderingDeviceDriver::MEMORY_ALLOCATION_TYPE_GPU,0);
  ERR_PRINT_ON;
  CHECK_FALSE(bool(buffer));CHECK(TestQualificationVulkanAllocationAccess::rollback(driver)==(stage==1?1:3));
  CHECK(TestQualificationVulkanAllocationAccess::allocations(driver)==before);
  // RED may leave native objects; restore test isolation only after recording
  // the actual production failure. This is not counted as driver rollback.
  TestQualificationVulkanAllocationAccess::clean_red_prefix(driver);
 }
}
#endif
