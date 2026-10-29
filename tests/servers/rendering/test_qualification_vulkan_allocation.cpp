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
 format.samples=RDD::TEXTURE_SAMPLES_1;
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
 format.samples=RDD::TEXTURE_SAMPLES_1;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 RenderingDeviceDriver::TextureView invalid_view;
 const uint32_t before=TestQualificationVulkanAllocationAccess::allocations(driver);
 ERR_PRINT_OFF;
 auto texture=driver.texture_create(format,invalid_view);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(texture));
 CHECK(TestQualificationVulkanAllocationAccess::allocations(driver)==before);
}
TEST_CASE("[QualificationNative][Vulkan] texture creation rejects invalid view swizzles") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::TextureFormat format;
 format.format=RDD::DATA_FORMAT_R8_UNORM;format.width=format.height=format.depth=format.array_layers=format.mipmaps=1;
 format.samples=RDD::TEXTURE_SAMPLES_1;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 const uint32_t before=TestQualificationVulkanAllocationAccess::allocations(driver);
 for (uint32_t channel = 0; channel < 4; channel++) {
  RenderingDeviceDriver::TextureView view;
  view.format=format.format;
  switch (channel) {
   case 0: view.swizzle_r=static_cast<RenderingDeviceDriver::TextureSwizzle>(RDD::TEXTURE_SWIZZLE_MAX); break;
   case 1: view.swizzle_g=static_cast<RenderingDeviceDriver::TextureSwizzle>(RDD::TEXTURE_SWIZZLE_MAX); break;
   case 2: view.swizzle_b=static_cast<RenderingDeviceDriver::TextureSwizzle>(RDD::TEXTURE_SWIZZLE_MAX); break;
   case 3: view.swizzle_a=static_cast<RenderingDeviceDriver::TextureSwizzle>(RDD::TEXTURE_SWIZZLE_MAX); break;
  }
  ERR_PRINT_OFF;
  auto texture=driver.texture_create(format,view);
  ERR_PRINT_ON;
  CHECK_FALSE(bool(texture));
  if (texture) {
   driver.texture_free(texture);
  }
 }
 CHECK(TestQualificationVulkanAllocationAccess::allocations(driver)==before);
}
TEST_CASE("[QualificationNative][Vulkan] direct format queries reject an invalid format") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 auto usages=driver.texture_get_usages_supported_by_format(RDD::DATA_FORMAT_MAX,false);
 bool filter_supported=driver.sampler_is_format_supported_for_filter(RDD::DATA_FORMAT_MAX,RDD::SAMPLER_FILTER_LINEAR);
 bool raw_reinterpretation=true;
 bool can_share=driver.texture_can_make_shared_with_format(RenderingDeviceDriver::TextureID(),RDD::DATA_FORMAT_MAX,raw_reinterpretation);
 auto extension=driver.texture_create_from_extension(0,RDD::TEXTURE_TYPE_2D,RDD::DATA_FORMAT_MAX,1,false,1);
 ERR_PRINT_ON;
 CHECK(usages==0);
 CHECK_FALSE(filter_supported);
 CHECK_FALSE(can_share);
 CHECK_FALSE(raw_reinterpretation);
 CHECK_FALSE(bool(extension));
}
TEST_CASE("[QualificationNative][Vulkan] vertex format creation rejects an invalid format") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::VertexAttribute attributes[1];
 RenderingDeviceDriver::VertexAttributeBindingsMap bindings;
 ERR_PRINT_OFF;
 auto vertex_format=driver.vertex_format_create(Span<RenderingDeviceDriver::VertexAttribute>(attributes,1),bindings);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(vertex_format));
}
TEST_CASE("[QualificationNative][Vulkan] direct buffer format rejects invalid input") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 bool result=driver.buffer_set_texel_format(RenderingDeviceDriver::BufferID(),RDD::DATA_FORMAT_MAX);
 ERR_PRINT_ON;
 CHECK_FALSE(result);
}
TEST_CASE("[QualificationNative][Vulkan] sampler creation rejects invalid filter") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::SamplerState state;
 state.mag_filter=static_cast<RenderingDeviceDriver::SamplerFilter>(2);
 ERR_PRINT_OFF;
 auto sampler=driver.sampler_create(state);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(sampler));
 if (sampler) {
  driver.sampler_free(sampler);
 }
}
TEST_CASE("[QualificationNative][Vulkan] sampler creation rejects invalid repeat mode") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::SamplerState state;
 state.repeat_u=RDD::SAMPLER_REPEAT_MODE_MAX;
 ERR_PRINT_OFF;
 auto sampler=driver.sampler_create(state);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(sampler));
 if (sampler) {
  driver.sampler_free(sampler);
 }
}
TEST_CASE("[QualificationNative][Vulkan] sampler creation rejects invalid compare op") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::SamplerState state;
 state.enable_compare=true;
 state.compare_op=RDD::COMPARE_OP_MAX;
 ERR_PRINT_OFF;
 auto sampler=driver.sampler_create(state);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(sampler));
 if (sampler) {
  driver.sampler_free(sampler);
 }
}
TEST_CASE("[QualificationNative][Vulkan] sampler creation rejects invalid border color") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::SamplerState state;
 state.border_color=RDD::SAMPLER_BORDER_COLOR_MAX;
 ERR_PRINT_OFF;
 auto sampler=driver.sampler_create(state);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(sampler));
 if (sampler) {
  driver.sampler_free(sampler);
 }
}
TEST_CASE("[QualificationNative][Vulkan] index binding rejects invalid format before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_render_bind_index_buffer(RenderingDeviceDriver::CommandBufferID(),RenderingDeviceDriver::BufferID(),static_cast<RenderingDeviceDriver::IndexBufferFormat>(2),0);
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] vertex binding rejects missing arrays before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_render_bind_vertex_buffers(RenderingDeviceDriver::CommandBufferID(),1,nullptr,nullptr,0);
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] attachment clear rejects missing views before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_render_clear_attachments(RenderingDeviceDriver::CommandBufferID(),VectorView<RenderingDeviceDriver::AttachmentClear>(nullptr,1),{});
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] viewport binding rejects missing view before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_render_set_viewport(RenderingDeviceDriver::CommandBufferID(),VectorView<Rect2i>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] scissor binding rejects missing view before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_render_set_scissor(RenderingDeviceDriver::CommandBufferID(),VectorView<Rect2i>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] push constants reject missing data before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_bind_push_constants(RenderingDeviceDriver::CommandBufferID(),RenderingDeviceDriver::ShaderID(),0,VectorView<uint32_t>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] render uniform binding rejects missing sets before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_bind_render_uniform_sets(RenderingDeviceDriver::CommandBufferID(),VectorView<RenderingDeviceDriver::UniformSetID>(nullptr,1),RenderingDeviceDriver::ShaderID(),0,1,0);
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] compute uniform binding rejects missing sets before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_bind_compute_uniform_sets(RenderingDeviceDriver::CommandBufferID(),VectorView<RenderingDeviceDriver::UniformSetID>(nullptr,1),RenderingDeviceDriver::ShaderID(),0,1,0);
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] secondary command execution rejects missing view before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_buffer_execute_secondary(RenderingDeviceDriver::CommandBufferID(),VectorView<RenderingDeviceDriver::CommandBufferID>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] render pass begin rejects missing clear values before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_begin_render_pass(RenderingDeviceDriver::CommandBufferID(),RenderingDeviceDriver::RenderPassID(),RenderingDeviceDriver::FramebufferID(),RenderingDeviceDriver::COMMAND_BUFFER_TYPE_PRIMARY,Rect2i(),VectorView<RenderingDeviceDriver::RenderPassClearValue>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] pipeline barrier rejects missing views before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_pipeline_barrier(RenderingDeviceDriver::CommandBufferID(),{}, {},VectorView<RenderingDeviceDriver::MemoryAccessBarrier>(nullptr,1),{}, {},{});
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] queue submit rejects missing wait view before queue access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 const Error result=driver.command_queue_execute_and_present(RenderingDeviceDriver::CommandQueueID(),VectorView<RenderingDeviceDriver::SemaphoreID>(nullptr,1),{}, {},RenderingDeviceDriver::FenceID(),{});
 ERR_PRINT_ON;
 CHECK(result==ERR_INVALID_PARAMETER);
}
TEST_CASE("[QualificationNative][Vulkan] buffer copy rejects missing region view before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_copy_buffer(RenderingDeviceDriver::CommandBufferID(),RenderingDeviceDriver::BufferID(),RenderingDeviceDriver::BufferID(),VectorView<RenderingDeviceDriver::BufferCopyRegion>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] texture copy rejects missing region view before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_copy_texture(RenderingDeviceDriver::CommandBufferID(),RenderingDeviceDriver::TextureID(),RenderingDeviceDriver::TEXTURE_LAYOUT_UNDEFINED,RenderingDeviceDriver::TextureID(),RenderingDeviceDriver::TEXTURE_LAYOUT_UNDEFINED,VectorView<RenderingDeviceDriver::TextureCopyRegion>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] buffer-to-texture copy rejects missing region view before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_copy_buffer_to_texture(RenderingDeviceDriver::CommandBufferID(),RenderingDeviceDriver::BufferID(),RenderingDeviceDriver::TextureID(),RenderingDeviceDriver::TEXTURE_LAYOUT_UNDEFINED,VectorView<RenderingDeviceDriver::BufferTextureCopyRegion>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] texture-to-buffer copy rejects missing region view before command access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 driver.command_copy_texture_to_buffer(RenderingDeviceDriver::CommandBufferID(),RenderingDeviceDriver::TextureID(),RenderingDeviceDriver::TEXTURE_LAYOUT_UNDEFINED,RenderingDeviceDriver::BufferID(),VectorView<RenderingDeviceDriver::BufferTextureCopyRegion>(nullptr,1));
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] texture creation rejects an invalid shareable format") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::TextureFormat format;
 format.format=RDD::DATA_FORMAT_R8_UNORM;format.width=format.height=format.depth=format.array_layers=format.mipmaps=1;
 format.samples=RDD::TEXTURE_SAMPLES_1;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 format.shareable_formats.push_back(RDD::DATA_FORMAT_MAX);
 RenderingDeviceDriver::TextureView view;view.format=format.format;
 ERR_PRINT_OFF;
 auto texture=driver.texture_create(format,view);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(texture));
}
TEST_CASE("[QualificationNative][Vulkan] texture creation rejects an invalid base format") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::TextureFormat format;
 format.format=RDD::DATA_FORMAT_MAX;format.width=format.height=format.depth=format.array_layers=format.mipmaps=1;
 format.samples=RDD::TEXTURE_SAMPLES_1;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 RenderingDeviceDriver::TextureView view;view.format=RDD::DATA_FORMAT_R8_UNORM;
 ERR_PRINT_OFF;
 auto texture=driver.texture_create(format,view);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(texture));
}
TEST_CASE("[QualificationNative][Vulkan] render pass creation rejects an invalid attachment format") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::Attachment attachment;
 ERR_PRINT_OFF;
 auto render_pass=driver.render_pass_create(VectorView<RenderingDeviceDriver::Attachment>(attachment),{}, {},1,RenderingDeviceDriver::AttachmentReference());
 ERR_PRINT_ON;
 CHECK_FALSE(bool(render_pass));
}
TEST_CASE("[QualificationNative][Vulkan] render pass creation rejects an invalid attachment layout") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::Attachment attachment;
 attachment.format=RDD::DATA_FORMAT_R8_UNORM;
 attachment.initial_layout=static_cast<RenderingDeviceDriver::TextureLayout>(RDD::TEXTURE_LAYOUT_MAX);
 ERR_PRINT_OFF;
 auto render_pass=driver.render_pass_create(VectorView<RenderingDeviceDriver::Attachment>(attachment),{}, {},1,RenderingDeviceDriver::AttachmentReference());
 ERR_PRINT_ON;
 CHECK_FALSE(bool(render_pass));
}
TEST_CASE("[QualificationNative][Vulkan] texture creation rejects an invalid sample count") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::TextureFormat format;
 format.format=RDD::DATA_FORMAT_R8_UNORM;format.width=format.height=format.depth=format.array_layers=format.mipmaps=1;
 format.samples=RDD::TEXTURE_SAMPLES_MAX;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 RenderingDeviceDriver::TextureView view;view.format=format.format;
 const uint32_t before=TestQualificationVulkanAllocationAccess::allocations(driver);
 ERR_PRINT_OFF;
 auto texture=driver.texture_create(format,view);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(texture));
 CHECK(TestQualificationVulkanAllocationAccess::allocations(driver)==before);
}
TEST_CASE("[QualificationNative][Vulkan] texture creation rejects an invalid texture type") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::TextureFormat format;
 format.format=RDD::DATA_FORMAT_R8_UNORM;format.width=format.height=format.depth=format.array_layers=format.mipmaps=1;
 format.samples=RDD::TEXTURE_SAMPLES_1;
 format.texture_type=RDD::TEXTURE_TYPE_MAX;
 format.usage_bits=RDD::TEXTURE_USAGE_SAMPLING_BIT;
 RenderingDeviceDriver::TextureView view;view.format=format.format;
 const uint32_t before=TestQualificationVulkanAllocationAccess::allocations(driver);
 ERR_PRINT_OFF;
 auto texture=driver.texture_create(format,view);
 ERR_PRINT_ON;
 CHECK_FALSE(bool(texture));
 CHECK(TestQualificationVulkanAllocationAccess::allocations(driver)==before);
}
TEST_CASE("[QualificationNative][Vulkan] texture layout commands reject invalid layouts before resource access") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 const auto invalid_layout=static_cast<RenderingDeviceDriver::TextureLayout>(RDD::TEXTURE_LAYOUT_MAX);
 RenderingDeviceDriver::TextureBarrier barrier;
 barrier.prev_layout=invalid_layout;
 ERR_PRINT_OFF;
 driver.command_pipeline_barrier(RenderingDeviceDriver::CommandBufferID(),{}, {}, {}, {}, VectorView<RenderingDeviceDriver::TextureBarrier>(barrier),{});
 driver.command_copy_texture(RenderingDeviceDriver::CommandBufferID(),{},invalid_layout,{},RDD::TEXTURE_LAYOUT_UNDEFINED,{});
 driver.command_resolve_texture(RenderingDeviceDriver::CommandBufferID(),{},invalid_layout,0,0,{},RDD::TEXTURE_LAYOUT_UNDEFINED,0,0);
 driver.command_clear_color_texture(RenderingDeviceDriver::CommandBufferID(),{},invalid_layout,Color(),{});
 driver.command_clear_depth_stencil_texture(RenderingDeviceDriver::CommandBufferID(),{},invalid_layout,0.0f,0,{});
 driver.command_copy_buffer_to_texture(RenderingDeviceDriver::CommandBufferID(),{}, {},invalid_layout,{});
 driver.command_copy_texture_to_buffer(RenderingDeviceDriver::CommandBufferID(),{},invalid_layout,{},{});
 ERR_PRINT_ON;
 CHECK(true);
}
TEST_CASE("[QualificationNative][Vulkan] render pass creation rejects invalid view counts") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 ERR_PRINT_OFF;
 auto zero_views=driver.render_pass_create({}, {}, {},0,RenderingDeviceDriver::AttachmentReference());
 auto too_many_views=driver.render_pass_create({}, {}, {},32,RenderingDeviceDriver::AttachmentReference());
 ERR_PRINT_ON;
 CHECK_FALSE(bool(zero_views));
 CHECK_FALSE(bool(too_many_views));
}
TEST_CASE("[QualificationNative][Vulkan] BLAS creation rejects an invalid vertex format") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::AccelerationStructureGeometry geometry;
 geometry.vertex_format=RDD::DATA_FORMAT_MAX;
 ERR_PRINT_OFF;
 auto blas=driver.blas_create(VectorView<RenderingDeviceDriver::AccelerationStructureGeometry>(geometry),{});
 ERR_PRINT_ON;
 CHECK_FALSE(bool(blas));
}
TEST_CASE("[QualificationNative][Vulkan] render pass creation rejects an invalid sample count") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::Attachment attachment;
 attachment.format=RDD::DATA_FORMAT_R8_UNORM;
 attachment.samples=RDD::TEXTURE_SAMPLES_MAX;
 ERR_PRINT_OFF;
 auto render_pass=driver.render_pass_create(VectorView<RenderingDeviceDriver::Attachment>(attachment),{}, {},1,RenderingDeviceDriver::AttachmentReference());
 ERR_PRINT_ON;
 CHECK_FALSE(bool(render_pass));
}
TEST_CASE("[QualificationNative][Vulkan] render pipeline creation rejects invalid primitive and sample count") {
 RenderingContextDriverVulkan context;REQUIRE(context.initialize()==OK);
 RenderingDeviceDriverVulkan driver(&context);REQUIRE(driver.initialize(0,2)==OK);
 RenderingDeviceDriver::PipelineRasterizationState rasterization;
 RenderingDeviceDriver::PipelineMultisampleState multisample;
 RenderingDeviceDriver::PipelineDepthStencilState depth_stencil;
 RenderingDeviceDriver::PipelineColorBlendState blend;
 ERR_PRINT_OFF;
 auto invalid_primitive=driver.render_pipeline_create(RenderingDeviceDriver::ShaderID(),RenderingDeviceDriver::VertexFormatID(),static_cast<RenderingDeviceDriver::RenderPrimitive>(RDD::RENDER_PRIMITIVE_MAX),rasterization,multisample,depth_stencil,blend,{}, {},RenderingDeviceDriver::RenderPassID(),0,{});
 multisample.sample_count=RDD::TEXTURE_SAMPLES_MAX;
 auto invalid_sample=driver.render_pipeline_create(RenderingDeviceDriver::ShaderID(),RenderingDeviceDriver::VertexFormatID(),RDD::RENDER_PRIMITIVE_TRIANGLES,rasterization,multisample,depth_stencil,blend,{}, {},RenderingDeviceDriver::RenderPassID(),0,{});
 ERR_PRINT_ON;
 CHECK_FALSE(bool(invalid_primitive));
 CHECK_FALSE(bool(invalid_sample));
}
#endif
