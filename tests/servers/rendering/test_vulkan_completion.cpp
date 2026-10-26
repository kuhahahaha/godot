/**************************************************************************/
/*  test_vulkan_completion.cpp                                            */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_vulkan_completion)

#if defined(VULKAN_ENABLED) && defined(MACOS_ENABLED)
#include "core/extension/gdextension_library_exit_guard.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#include "drivers/vulkan/rendering_device_driver_vulkan.h"
#include "servers/rendering/rendering_device.h"

class TestVulkanCompletionAccess {
public:
	static void lock_calls(RenderingDeviceDriverVulkan &driver) { driver.native_call_mutex.lock(); }
	static void unlock_calls(RenderingDeviceDriverVulkan &driver) { driver.native_call_mutex.unlock(); }
	static uint64_t next_serial(RenderingDeviceDriverVulkan &driver) { return driver.next_native_call_serial.load(); }
	static void set_next_serial(RenderingDeviceDriverVulkan &driver, uint64_t serial) { driver.next_native_call_serial.store(serial); }
	static uint32_t set_observers(RenderingDeviceDriverVulkan &driver, uint64_t serial, uint32_t count) {
		MutexLock lock(driver.native_call_mutex);
		for (auto *call : driver.native_calls) {
			if (call->serial == serial) {
				const auto old = call->observers;
				call->observers = count;
				return old;
			}
		}
		return 0;
	}
	static void settle_injected_submit(RenderingDeviceDriverVulkan &driver, uint64_t serial) {
		MutexLock lock(driver.native_call_mutex);
		for (auto *call : driver.native_calls) {
			if (call->serial == serial) {
				// Fixture teardown only: the injected error follows an actual
				// successful submit. Wait for its actual source before restoring
				// metadata; this is outside the Unknown-retention oracle.
				const auto result = vkWaitForFences(driver.vk_device, 1, &call->fence, VK_TRUE, 5000000000ULL);
				CRASH_COND_MSG(result != VK_SUCCESS, "Injected-submit fixture failed to settle its actual source.");
				call->submit_result = VK_SUCCESS;
				return;
			}
		}
		FAIL_CHECK("injected native source was discarded");
	}
	static bool recycle_waiting(RenderingDeviceDriverVulkan &driver, uint64_t serial) { return driver.test_native_recycle_cv_waiting.load() == serial; }
	static void set_fixture_waiters(RenderingDeviceDriverVulkan &driver, uint64_t serial, uint32_t count) {
		MutexLock lock(driver.native_call_mutex);
		for (auto *call : driver.native_calls) {
			if (call->serial == serial) {
				call->waiters = count;
				return;
			}
		}
		FAIL_CHECK("fixture source disappeared before its return");
	}
	static void return_fixture_source(RenderingDeviceDriverVulkan &driver, RenderingDeviceDriver::FenceID id) {
		MutexLock lock(driver.native_call_mutex);
		auto *fence = (RenderingDeviceDriverVulkan::Fence *)id.id;
		CRASH_COND(!fence->native_call);
		fence->native_call->waiters = 0;
		const auto result = vkResetFences(driver.vk_device, 1, &fence->vk_fence);
		CRASH_COND(result != VK_SUCCESS);
		driver._native_fence_return_locked(fence);
		fence->completed_serial = fence->submission_serial;
		fence->armed = false;
		fence->observed = false;
	}
	static void notify_returned(RenderingDeviceDriverVulkan &driver) { driver.native_call_returned.notify_all(); }
	static void cleanup_idle(RenderingDeviceDriverVulkan &driver) { CHECK(vkDeviceWaitIdle(driver.vk_device) == VK_SUCCESS); }
	static Error create_owner(RenderingDevice &owner) { return GDExtensionDeviceExit::Registry::get_singleton().device_created(&owner); }
	static Error admission(RenderingDevice &owner) {
		auto thread = Thread::get_caller_id();
		return GDExtensionDeviceExit::Registry::get_singleton().device_execution_moved(&owner, thread, thread);
	}
	static void identity(RenderingDevice &owner, uint64_t &registry, uint64_t &epoch) { GDExtensionDeviceExit::Registry::get_singleton().device_loss_identity(&owner, registry, epoch); }
	static bool submit_fault_applied(RenderingDeviceDriverVulkan &driver) { return driver.test_submit_fault_applied; }
	static void fault(RenderingDeviceDriverVulkan &driver, uint32_t phase, VkResult result) {
		if (phase == 4) {
			driver.test_submit_fault_applied = false;
		}
		driver.test_fence_fault_phase = phase;
		driver.test_fence_fault_result = result;
	}
	static void bind_owner(RenderingDeviceDriverVulkan &driver, RenderingDevice *owner) { driver.session_owner = owner; }
	static void arm(RenderingDeviceDriver::FenceID id) {
		auto *fence = (RenderingDeviceDriverVulkan::Fence *)id.id;
		fence->submission_serial = 7;
		fence->armed = true;
		fence->observed = true;
	}
	static void exhaust(RenderingDeviceDriver::FenceID p_fence) {
		auto *fence = (RenderingDeviceDriverVulkan::Fence *)p_fence.id;
		fence->submission_serial = UINT64_MAX;
	}
};

TEST_CASE("[K03][VulkanCompletion] native batch identity survives mismatch and exact recycle") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	const auto buffer = driver.buffer_create(4, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	const auto fence = driver.fence_create();
	REQUIRE(bool(buffer));
	for (int attempt = 0; attempt < 2; attempt++) {
		REQUIRE(driver.command_buffer_begin(command));
		driver.command_clear_buffer(command, buffer, 0, 4);
		driver.command_buffer_end(command);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
		const auto report = driver.get_last_queue_call_report();
		REQUIRE(report.issued);
		REQUIRE(report.submission_serial != 0);
		const auto wrong = driver.fence_poll(fence, report.submission_serial + 1);
		CHECK_FALSE(wrong.matches(report.submission_serial + 1));
		CHECK(wrong.completed_serial == report.submission_serial);
		const auto completion = driver.fence_wait_observed(fence, report.submission_serial);
		REQUIRE(completion.matches(report.submission_serial));
		REQUIRE(driver.fence_recycle(fence, report.submission_serial) == OK);
		CHECK(driver.fence_recycle(fence, report.submission_serial) != OK);
		REQUIRE(driver.command_pool_reset(pool));
	}
	TestVulkanCompletionAccess::exhaust(fence);
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_clear_buffer(command, buffer, 0, 4);
	driver.command_buffer_end(command);
	const Error exhausted = driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {});
	CHECK(exhausted == ERR_UNAVAILABLE);
	CHECK_FALSE(driver.get_last_queue_call_report().issued);
	CHECK(driver.get_last_queue_call_report().error == ERR_UNAVAILABLE);
	// The baseline can submit before noticing serial wrap, so wait natively
	// for fixture safety without using zero as a typed completion identity.
	if (exhausted == OK) {
		REQUIRE(driver.fence_wait(fence) == OK);
	}
	driver.fence_free(fence);
	driver.buffer_free(buffer);
	driver.command_pool_free(pool);
}

namespace {
// Only the external Vulkan status is injected. Driver result handling and the
// original device registry/admission remain real; no hardware loss is claimed.
enum class FenceFaultPath { Poll,
	ObservedWait,
	LegacyQuery,
	LegacyWait,
	LegacyReset,
	Recycle };
struct FenceFault {
	RenderingDeviceDriverVulkan &driver;
	FenceFault(RenderingDeviceDriverVulkan &p_driver, FenceFaultPath path, VkResult result) : driver(p_driver) {
		const uint32_t phase = (path == FenceFaultPath::Recycle || path == FenceFaultPath::LegacyReset) ? 3 : (path == FenceFaultPath::ObservedWait || path == FenceFaultPath::LegacyWait ? 2 : 1);
		TestVulkanCompletionAccess::fault(driver, phase, result);
	}
	~FenceFault() { TestVulkanCompletionAccess::fault(driver, 0, VK_SUCCESS); }
};
} //namespace
TEST_CASE("[K03][VulkanCompletion] lost fence results revoke original epoch admission while other failures preserve it") {
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	using RDD = RenderingDeviceDriver;
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	for (auto path : { FenceFaultPath::Poll, FenceFaultPath::ObservedWait, FenceFaultPath::LegacyQuery, FenceFaultPath::LegacyWait, FenceFaultPath::LegacyReset, FenceFaultPath::Recycle }) {
		for (VkResult code : { VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_DEVICE_LOST }) {
			CAPTURE(int(path));
			CAPTURE(int(code));
			RenderingDevice owner;
			REQUIRE(TestVulkanCompletionAccess::create_owner(owner) == OK);
			TestVulkanCompletionAccess::bind_owner(driver, &owner);
			REQUIRE(TestVulkanCompletionAccess::admission(owner) == OK);
			uint64_t original_registry = 0, original_epoch = 0;
			TestVulkanCompletionAccess::identity(owner, original_registry, original_epoch);
			REQUIRE(original_registry != 0);
			REQUIRE(original_epoch != 0);
			const auto fence = driver.fence_create();
			REQUIRE(bool(fence));
			if (path == FenceFaultPath::LegacyReset) {
				// Reach reset after an actual successful native submission and
				// observation, rather than pretending an unsignaled fence is done.
				REQUIRE(driver.command_buffer_begin(command));
				driver.command_buffer_end(command);
				REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
				const auto serial = driver.get_last_queue_call_report().submission_serial;
				REQUIRE(driver.fence_wait_observed(fence, serial).matches(serial));
			} else {
				TestVulkanCompletionAccess::arm(fence);
			}
			{
				FenceFault fault(driver, path, code);
				if (path == FenceFaultPath::Poll || path == FenceFaultPath::ObservedWait) {
					const auto report = path == FenceFaultPath::Poll ? driver.fence_poll(fence, 7) : driver.fence_wait_observed(fence, 7);
					CHECK(report.native_error);
					CHECK_FALSE(report.completed);
					CHECK(report.native_code == int64_t(code));
					CHECK(report.completed_serial == 7);
				} else {
					ERR_PRINT_OFF;
					const auto result = path == FenceFaultPath::Recycle ? driver.fence_recycle(fence, 7) : driver.fence_wait(fence);
					ERR_PRINT_ON;
					CHECK(result == (path == FenceFaultPath::Recycle ? ERR_CANT_CREATE : FAILED));
				}
			}
			CHECK(TestVulkanCompletionAccess::admission(owner) == (code == VK_ERROR_DEVICE_LOST ? ERR_UNAUTHORIZED : OK));
			uint64_t observed_registry = 0, observed_epoch = 0;
			TestVulkanCompletionAccess::identity(owner, observed_registry, observed_epoch);
			CHECK(observed_registry == original_registry);
			CHECK(observed_epoch == original_epoch);
			TestVulkanCompletionAccess::bind_owner(driver, nullptr);
			driver.fence_free(fence);
			if (path == FenceFaultPath::LegacyReset) {
				REQUIRE(driver.command_pool_reset(pool));
			}
		}
	}
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] retained submit identity survives its original fence return without cross fence alias") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	uint64_t native[2] = {};
	for (unsigned i = 0; i < 2; ++i) {
		const auto fence = driver.fence_create();
		REQUIRE(bool(fence));
		REQUIRE(driver.command_buffer_begin(command));
		driver.command_buffer_end(command);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
		const auto report = driver.get_last_queue_call_report();
		REQUIRE(report.calls.size() == 1);
		native[i] = report.calls[0].serial;
		CHECK(native[i] != 0);
		CHECK(report.calls[0].completion_serial == native[i]);
		CHECK(driver.native_call_retain(native[i]) == OK);
		// Cleanup/return of the real original fence must leave the retained immutable
		// native observation accessible. This is not a different tail fence.
		REQUIRE(driver.fence_wait(fence) == OK);
		driver.fence_free(fence);
		CHECK(driver.native_call_poll(native[i]).matches(native[i]));
		if (i == 1) {
			CHECK(native[1] != native[0]);
			CHECK(driver.native_call_poll(native[0]).matches(native[0]));
		}
		REQUIRE(driver.command_pool_reset(pool));
	}
	for (uint64_t serial : native) {
		CHECK(driver.native_call_recycle(serial) == OK);
		CHECK(driver.native_call_release(serial) == OK);
		CHECK_FALSE(driver.native_call_poll(serial).matches(serial));
	}
	TestVulkanCompletionAccess::cleanup_idle(driver);
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}
TEST_CASE("[K03][VulkanNativeCall] submission without frame fence retains its own actual completion source") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, {}, {}) == OK);
	const auto report = driver.get_last_queue_call_report();
	REQUIRE(report.calls.size() == 1);
	const auto &fact = report.calls[0];
	CHECK(fact.serial != 0);
	CHECK(fact.completion != RDD::CompletionCapability::None);
	CHECK(fact.completion_serial == fact.serial);
	CHECK(driver.native_call_retain(fact.serial) == OK);
	const auto completed = driver.native_call_wait(fact.serial);
	CHECK(completed.matches(fact.completion_serial));
	CHECK_FALSE(completed.native_error);
	CHECK(driver.native_call_recycle(fact.serial) == OK);
	CHECK(driver.native_call_release(fact.serial) == OK);
	CHECK_FALSE(driver.native_call_poll(fact.serial).matches(fact.completion_serial));
	// RED still issued real GPU work before discovering missing observation.
	// Wait only for fixture cleanup; this does not satisfy the production oracle above.
	TestVulkanCompletionAccess::cleanup_idle(driver);
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] ordinary submission tolerates collection bookkeeping contention") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	struct Submit {
		RenderingDeviceDriverVulkan *driver;
		RDD::CommandQueueID queue;
		RDD::CommandBufferID command;
		std::atomic<bool> done{ false };
		Error result = FAILED;
	} submit{ &driver, queue, command };
	const auto before = TestVulkanCompletionAccess::next_serial(driver);
	TestVulkanCompletionAccess::lock_calls(driver);
	Thread thread;
	thread.start([](void *arg) {
		auto &s = *static_cast<Submit *>(arg);
		s.result = s.driver->command_queue_execute_and_present(s.queue, {}, s.command, {}, {}, {});
		s.done.store(true);
	},
			&submit);
	// Serial allocation precedes acquisition of the collection lock. Keep the
	// lock held long enough to expose the old non-retryable ERR_BUSY path.
	const auto deadline = OS::get_singleton()->get_ticks_msec() + 1000;
	while (!submit.done.load() && OS::get_singleton()->get_ticks_msec() < deadline) {
		OS::get_singleton()->delay_usec(1000);
	}
	const bool reached_prepare = TestVulkanCompletionAccess::next_serial(driver) != before;
	TestVulkanCompletionAccess::unlock_calls(driver);
	thread.wait_to_finish();
	CHECK(reached_prepare);
	CHECK(submit.result == OK);
	const auto report = driver.get_last_queue_call_report();
	CHECK(report.issued);
	if (report.issued && report.calls.size() == 1) {
		const auto serial = report.calls[0].serial;
		CHECK(driver.native_call_wait(serial).matches(serial));
		CHECK(driver.native_call_recycle(serial) == OK);
	}
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] same fence reuse preserves old snapshot across failed reset") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	const auto fence = driver.fence_create();
	REQUIRE(bool(fence));
	uint64_t previous = 0;
	for (unsigned attempt = 0; attempt < 2; ++attempt) {
		REQUIRE(driver.command_buffer_begin(command));
		driver.command_buffer_end(command);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
		const auto report = driver.get_last_queue_call_report();
		REQUIRE(report.calls.size() == 1);
		const auto serial = report.calls[0].serial;
		REQUIRE(serial != previous);
		REQUIRE(driver.native_call_retain(serial) == OK);
		REQUIRE(driver.native_call_wait(serial).matches(serial));
		REQUIRE(driver.fence_poll(fence, report.submission_serial).matches(report.submission_serial));
		{
			FenceFault fault(driver, FenceFaultPath::Recycle, VK_ERROR_OUT_OF_HOST_MEMORY);
			CHECK(driver.fence_recycle(fence, report.submission_serial) == ERR_CANT_CREATE);
		}
		// Completion is immutable even when source return failed. Reusing the
		// original fence is refused until its actual reset succeeds.
		CHECK(driver.native_call_poll(serial).matches(serial));
		CHECK(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == ERR_BUSY);
		CHECK_FALSE(driver.get_last_queue_call_report().issued);
		REQUIRE(driver.fence_recycle(fence, report.submission_serial) == OK);
		CHECK(driver.native_call_poll(serial).matches(serial));
		if (previous) {
			CHECK(driver.native_call_poll(previous).matches(previous));
			CHECK(driver.native_call_recycle(previous) == OK);
			CHECK(driver.native_call_release(previous) == OK);
		}
		previous = serial;
		REQUIRE(driver.command_pool_reset(pool));
	}
	driver.fence_free(fence);
	CHECK(driver.native_call_poll(previous).matches(previous));
	CHECK(driver.native_call_recycle(previous) == OK);
	CHECK(driver.native_call_release(previous) == OK);
	CHECK_FALSE(driver.native_call_poll(previous).matches(previous));
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] initial source loss observed after registration revokes its original owner") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	for (auto path : { FenceFaultPath::Poll, FenceFaultPath::ObservedWait }) {
		CAPTURE(int(path));
		RenderingDevice owner;
		TestVulkanCompletionAccess::bind_owner(driver, &owner);
		REQUIRE(driver.command_buffer_begin(command));
		driver.command_buffer_end(command);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, {}, {}) == OK);
		const auto report = driver.get_last_queue_call_report();
		REQUIRE(report.calls.size() == 1);
		const auto serial = report.calls[0].serial;
		REQUIRE(driver.native_call_retain(serial) == OK);
		// Source preparation preceded original registry registration. Native idle
		// here only keeps fault-injection cleanup safe; no cell observation is cached.
		TestVulkanCompletionAccess::cleanup_idle(driver);
		REQUIRE(TestVulkanCompletionAccess::create_owner(owner) == OK);
		REQUIRE(TestVulkanCompletionAccess::admission(owner) == OK);
		uint64_t registry = 0, epoch = 0;
		TestVulkanCompletionAccess::identity(owner, registry, epoch);
		REQUIRE(epoch != 0);
		{
			FenceFault fault(driver, path, VK_ERROR_DEVICE_LOST);
			const auto lost = path == FenceFaultPath::Poll ? driver.native_call_poll(serial) : driver.native_call_wait(serial);
			CHECK(lost.native_error);
			CHECK(lost.native_code == VK_ERROR_DEVICE_LOST);
			CHECK_FALSE(lost.completed);
		}
		CHECK(TestVulkanCompletionAccess::admission(owner) == ERR_UNAUTHORIZED);
		uint64_t after_registry = 0, after_epoch = 0;
		TestVulkanCompletionAccess::identity(owner, after_registry, after_epoch);
		CHECK(after_registry == registry);
		CHECK(after_epoch == epoch);
		// The injected status did not physically lose this device. Settle only the
		// original old source; completing it must not restore the lost owner's gate.
		CHECK(driver.native_call_wait(serial).matches(serial));
		CHECK(driver.native_call_recycle(serial) == OK);
		CHECK(driver.native_call_release(serial) == OK);
		CHECK(TestVulkanCompletionAccess::admission(owner) == ERR_UNAUTHORIZED);
		TestVulkanCompletionAccess::bind_owner(driver, nullptr);
		REQUIRE(driver.command_pool_reset(pool));
	}
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}
TEST_CASE("[K03][VulkanNativeCall] serial exhaustion rejects before issue and observer count refuses overflow") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	TestVulkanCompletionAccess::set_next_serial(driver, UINT64_MAX);
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, {}, {}) == OK);
	const auto report = driver.get_last_queue_call_report();
	REQUIRE(report.calls.size() == 1);
	const auto serial = report.calls[0].serial;
	CHECK(serial == UINT64_MAX);
	CHECK(TestVulkanCompletionAccess::next_serial(driver) == 0);
	REQUIRE(driver.native_call_retain(serial) == OK);
	CHECK(TestVulkanCompletionAccess::set_observers(driver, serial, UINT32_MAX) == 1);
	CHECK(driver.native_call_retain(serial) == ERR_PARAMETER_RANGE_ERROR);
	// Restore the original real observer count after checking arithmetic refusal.
	CHECK(TestVulkanCompletionAccess::set_observers(driver, serial, 1) == UINT32_MAX);
	CHECK(driver.native_call_wait(serial).matches(serial));
	CHECK(driver.native_call_release(serial) == OK);
	CHECK(driver.native_call_release(serial) == ERR_DOES_NOT_EXIST);
	// Releasing the final observer alone does not return the physical source.
	CHECK(driver.native_call_poll(serial).matches(serial));
	REQUIRE(driver.command_pool_reset(pool));
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	CHECK(driver.command_queue_execute_and_present(queue, {}, command, {}, {}, {}) == ERR_UNAVAILABLE);
	const auto refused = driver.get_last_queue_call_report();
	CHECK_FALSE(refused.issued);
	CHECK(refused.calls.is_empty());
	CHECK(refused.error == ERR_UNAVAILABLE);
	CHECK(driver.native_call_poll(serial).matches(serial));
	CHECK(driver.native_call_recycle(serial) == OK);
	CHECK_FALSE(driver.native_call_poll(serial).matches(serial));
	CHECK(driver.native_call_retain(serial) == ERR_DOES_NOT_EXIST);
	CHECK_FALSE(driver.native_call_poll(0).completed);
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] unknown submit retains original source without inferring completion") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	TestVulkanCompletionAccess::fault(driver, 4, VK_ERROR_OUT_OF_HOST_MEMORY);
	ERR_PRINT_OFF;
	const auto result = driver.command_queue_execute_and_present(queue, {}, command, {}, {}, {});
	ERR_PRINT_ON;
	TestVulkanCompletionAccess::fault(driver, 0, VK_SUCCESS);
	CHECK(TestVulkanCompletionAccess::submit_fault_applied(driver));
	CHECK(result == FAILED);
	const auto report = driver.get_last_queue_call_report();
	REQUIRE(report.calls.size() == 1);
	const auto &fact = report.calls[0];
	CHECK(fact.issue == RDD::IssueFact::IssuedUnknown);
	CHECK(fact.native_code == VK_ERROR_OUT_OF_HOST_MEMORY);
	CHECK(fact.error == FAILED);
	REQUIRE(driver.native_call_retain(fact.serial) == OK);
	for (const auto observed : { driver.native_call_poll(fact.serial), driver.native_call_wait(fact.serial) }) {
		CHECK(observed.native_error);
		CHECK(observed.native_code == VK_ERROR_OUT_OF_HOST_MEMORY);
		CHECK_FALSE(observed.completed);
		CHECK(observed.completed_serial == fact.serial);
	}
	CHECK(driver.native_call_recycle(fact.serial) == ERR_BUSY);
	CHECK(driver.native_call_release(fact.serial) == OK);
	// An observer release cannot retire an unresolved native source.
	CHECK(driver.native_call_retain(fact.serial) == OK);
	TestVulkanCompletionAccess::settle_injected_submit(driver, fact.serial);
	CHECK(driver.native_call_wait(fact.serial).matches(fact.serial));
	CHECK(driver.native_call_recycle(fact.serial) == OK);
	CHECK(driver.native_call_release(fact.serial) == OK);
	CHECK_FALSE(driver.native_call_poll(fact.serial).matches(fact.serial));
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] blocking fence recycle waits while bounded recycle preserves Busy") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	const auto fence = driver.fence_create();
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
	const auto report = driver.get_last_queue_call_report();
	REQUIRE(report.calls.size() == 1);
	const auto native = report.calls[0].serial;
	REQUIRE(driver.native_call_retain(native) == OK);
	REQUIRE(driver.fence_wait_observed(fence, report.submission_serial).matches(report.submission_serial));
	struct Recycle {
		RenderingDeviceDriverVulkan *driver;
		RDD::FenceID fence;
		uint64_t serial;
		Error bounded = FAILED, blocking = FAILED;
		std::atomic<bool> done{ false };
	} recycle{ &driver, fence, report.submission_serial };
	TestVulkanCompletionAccess::lock_calls(driver);
	Thread thread;
	thread.start([](void *arg) {
		auto &r = *static_cast<Recycle *>(arg);
		r.bounded = r.driver->fence_recycle(r.fence, r.serial);
		r.blocking = r.driver->fence_recycle_wait(r.fence, r.serial);
		r.done.store(true);
	},
			&recycle);
	const auto deadline = OS::get_singleton()->get_ticks_msec() + 1000;
	while (!recycle.done.load() && OS::get_singleton()->get_ticks_msec() < deadline) {
		OS::get_singleton()->delay_usec(1000);
	}
	TestVulkanCompletionAccess::unlock_calls(driver);
	thread.wait_to_finish();
	CHECK(recycle.bounded == ERR_BUSY);
	CHECK(recycle.blocking == OK);
	CHECK(driver.native_call_poll(native).matches(native));
	CHECK(driver.native_call_recycle(native) == OK);
	CHECK(driver.native_call_release(native) == OK);
	driver.fence_free(fence);
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] blocking intermediate source recycle waits while bounded recycle preserves Busy") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, {}, {}) == OK);
	const auto report = driver.get_last_queue_call_report();
	REQUIRE(report.calls.size() == 1);
	const auto serial = report.calls[0].serial;
	REQUIRE(driver.native_call_retain(serial) == OK);
	REQUIRE(driver.native_call_wait(serial).matches(serial));
	struct Recycle {
		RenderingDeviceDriverVulkan *driver;
		uint64_t serial;
		Error bounded = FAILED, blocking = FAILED;
		std::atomic<bool> done{ false };
	} recycle{ &driver, serial };
	TestVulkanCompletionAccess::lock_calls(driver);
	Thread thread;
	thread.start([](void *arg) {
		auto &r = *static_cast<Recycle *>(arg);
		r.bounded = r.driver->native_call_recycle(r.serial);
		r.blocking = r.driver->native_call_recycle_wait(r.serial);
		r.done.store(true);
	},
			&recycle);
	const auto deadline = OS::get_singleton()->get_ticks_msec() + 1000;
	while (!recycle.done.load() && OS::get_singleton()->get_ticks_msec() < deadline) {
		OS::get_singleton()->delay_usec(1000);
	}
	TestVulkanCompletionAccess::unlock_calls(driver);
	thread.wait_to_finish();
	CHECK(recycle.bounded == ERR_BUSY);
	CHECK(recycle.blocking == OK);
	CHECK(driver.native_call_poll(serial).matches(serial));
	CHECK(driver.native_call_release(serial) == OK);
	CHECK_FALSE(driver.native_call_poll(serial).matches(serial));
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

TEST_CASE("[K03][VulkanNativeCall] waiting recycle revalidates source after original fence return") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverVulkan context;
	REQUIRE(context.initialize() == OK);
	RenderingDeviceDriverVulkan driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto family = driver.command_queue_family_get(RDD::COMMAND_QUEUE_FAMILY_TRANSFER_BIT);
	const auto queue = driver.command_queue_create(family);
	const auto pool = driver.command_pool_create(family, RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	const auto fence = driver.fence_create();
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_buffer_end(command);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
	const auto report = driver.get_last_queue_call_report();
	REQUIRE(report.calls.size() == 1);
	const auto serial = report.calls[0].serial;
	REQUIRE(driver.fence_wait_observed(fence, report.submission_serial).matches(report.submission_serial));
	// Real source and actual completion/reset; only waiter residency is controlled
	// by this fixture. No claim of a physical GPU wait being delayed is made.
	TestVulkanCompletionAccess::set_fixture_waiters(driver, serial, 1);
	struct Recycle {
		RenderingDeviceDriverVulkan *driver;
		uint64_t serial;
		Error result = FAILED;
	} recycle{ &driver, serial };
	Thread thread;
	thread.start([](void *arg) { auto &r = *static_cast<Recycle *>(arg); r.result = r.driver->native_call_recycle_wait(r.serial); }, &recycle);
	const auto deadline = OS::get_singleton()->get_ticks_msec() + 3000;
	while (!TestVulkanCompletionAccess::recycle_waiting(driver, serial) && OS::get_singleton()->get_ticks_msec() < deadline) {
		OS::get_singleton()->delay_usec(1000);
	}
	const bool waiting = TestVulkanCompletionAccess::recycle_waiting(driver, serial);
	// Reset the actual original fence and invoke its production source-return
	// routine in one critical section. Even a spurious wake sees only the fully
	// returned source. No observer pins it, so the helper must relookup by serial.
	TestVulkanCompletionAccess::return_fixture_source(driver, fence);
	CHECK_FALSE(driver.native_call_poll(serial).matches(serial));
	TestVulkanCompletionAccess::notify_returned(driver);
	thread.wait_to_finish();
	CHECK(waiting);
	CHECK(recycle.result == ERR_DOES_NOT_EXIST);
	driver.fence_free(fence);
	driver.command_pool_free(pool);
	driver.command_queue_free(queue);
}

#endif
