/**************************************************************************/
/*  test_vulkan_fence_wait.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef VULKAN_ENABLED
#include "drivers/vulkan/vulkan_fence_wait.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_capture_vulkan_fence_wait)
TEST_CASE("[P01] Vulkan native query errors cannot reset a fence or become successful waits") {
	for (VkResult query : { VK_ERROR_DEVICE_LOST, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY }) {
		int queries = 0, waits = 0, resets = 0;
		const auto result = vulkan_fence_wait_and_reset([&] { queries++; return query; }, [&] { waits++; return VK_SUCCESS; }, [&] { resets++; return VK_SUCCESS; });
		CHECK(result.phase == VulkanFenceWaitResult::QUERY);
		CHECK(result.result == query);
		CHECK(queries == 1);
		CHECK(waits == 0);
		CHECK(resets == 0);
	}
}
TEST_CASE("[P01] Vulkan legacy fence path preserves wait and reset failures") {
	for (VkResult query : { VK_SUCCESS, VK_NOT_READY }) {
		for (VkResult waited : { VK_SUCCESS, VK_ERROR_DEVICE_LOST, VK_TIMEOUT }) {
			for (VkResult reset : { VK_SUCCESS, VK_ERROR_OUT_OF_HOST_MEMORY }) {
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
