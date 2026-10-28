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
 static void texture_fail(RenderingDeviceDriverVulkan &driver,uint32_t stage){driver.test_texture_fault_stage=stage;}
 static uint32_t texture_rollback(RenderingDeviceDriverVulkan &driver){return driver.test_texture_rollback_mask;}
 static void texture_reset(RenderingDeviceDriverVulkan &driver){driver.test_texture_fault_stage=0;driver.test_texture_rollback_mask=0;driver.test_texture_prefix=VK_NULL_HANDLE;driver.test_texture_allocation_prefix=nullptr;}
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
TEST_CASE("[QualificationNative][Vulkan] failed extra-tracking texture creation rolls back image and memory") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::TextureFormat format;
 format.format=RDD::DATA_FORMAT_R8_UNORM;format.width=format.height=format.depth=format.array_layers=format.mipmaps=1;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 RenderingDeviceDriver::TextureView view;
 view.format = format.format;
 for(uint32_t stage=1;stage<=3;++stage) {
  const uint32_t before=TestQualificationVulkanAllocationAccess::allocations(driver);TestQualificationVulkanAllocationAccess::texture_fail(driver,stage);
  ERR_PRINT_OFF;
  auto texture=driver.texture_create(format,view);
  ERR_PRINT_ON;
  CHECK_FALSE(bool(texture));CHECK(TestQualificationVulkanAllocationAccess::texture_rollback(driver)==(stage==1?1:3));
  CHECK(TestQualificationVulkanAllocationAccess::allocations(driver)==before);
  TestQualificationVulkanAllocationAccess::texture_reset(driver);
 }
}
TEST_CASE("[QualificationNative][Vulkan] texture creation rejects an invalid direct view format") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::TextureFormat format;
 format.format=RDD::DATA_FORMAT_R8_UNORM;format.width=format.height=format.depth=format.array_layers=format.mipmaps=1;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 RenderingDeviceDriver::TextureView invalid_view;
 const uint32_t before=TestQualificationVulkanAllocationAccess::allocations(driver);
 ERR_PRINT_OFF;
 auto texture=driver.texture_create(format,invalid_view);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(texture));
 CHECK(TestQualificationVulkanAllocationAccess::allocations(driver)==before);
}
#endif
