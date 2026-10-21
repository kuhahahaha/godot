#pragma once
#ifdef VULKAN_ENABLED
#include "drivers/vulkan/vulkan_fence_wait.h"
#include "tests/test_macros.h"
TEST_CASE("[P01] Vulkan native query errors cannot reset a fence or become successful waits") {
	for (VkResult query : {VK_ERROR_DEVICE_LOST, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY}) {
		int queries = 0, waits = 0, resets = 0;
		const auto result = vulkan_fence_wait_and_reset([&] { queries++; return query; }, [&] { waits++; return VK_SUCCESS; }, [&] { resets++; return VK_SUCCESS; });
		CHECK(result.phase == VulkanFenceWaitResult::QUERY);
		CHECK(result.result == query);
		CHECK(queries == 1); CHECK(waits == 0); CHECK(resets == 0);
	}
}
TEST_CASE("[P01] Vulkan legacy fence path preserves wait and reset failures") {
	for (VkResult query : {VK_SUCCESS, VK_NOT_READY}) {
		for (VkResult waited : {VK_SUCCESS, VK_ERROR_DEVICE_LOST, VK_TIMEOUT}) {
			for (VkResult reset : {VK_SUCCESS, VK_ERROR_OUT_OF_HOST_MEMORY}) {
				int waits = 0, resets = 0;
				const auto result = vulkan_fence_wait_and_reset([&] { return query; }, [&] { waits++; return waited; }, [&] { resets++; return reset; });
				const bool wait_failed = query == VK_NOT_READY && waited != VK_SUCCESS;
				CHECK(waits == int(query == VK_NOT_READY));
				CHECK(resets == int(!wait_failed));
				CHECK(result.phase == (wait_failed ? VulkanFenceWaitResult::WAIT : VulkanFenceWaitResult::RESET));
				CHECK(result.result == (wait_failed ? waited : reset));
			}
		}
	}
}
#endif
