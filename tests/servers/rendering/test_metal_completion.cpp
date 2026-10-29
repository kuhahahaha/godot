/**************************************************************************/
/*  test_metal_completion.cpp                                              */
/**************************************************************************/

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_metal_completion)

#if defined(METAL_ENABLED) && defined(MACOS_ENABLED)
#include "drivers/metal/rendering_context_driver_metal.h"
#include "drivers/metal/rendering_device_driver_metal3.h"

class TestMetalCompletionAccess {
public:
	static uint32_t records(MTL3::RenderingDeviceDriverMetal &p_driver) {
		MutexLock lock(p_driver.completion_records_mutex);
		return p_driver.completion_records.size();
	}
	static void exhaust(RenderingDeviceDriver::FenceID p_fence) {
		auto *fence = (MTL3::RenderingDeviceDriverMetal::Fence *)p_fence.id;
		fence->value = UINT64_MAX;
	}
	static void fail_observed_batch(RenderingDeviceDriver::FenceID p_fence, int64_t p_code) {
		auto *fence = (MTL3::RenderingDeviceDriverMetal::Fence *)p_fence.id;
		for (auto *record : fence->completion_records) {
			REQUIRE(record->completed.load());
			record->native_code.store(p_code);
			record->failed.store(true);
		}
	}
	static void drain(MTL3::RenderingDeviceDriverMetal &p_driver) { p_driver.drain_completion_records(); }
	static void mode(MTL3::RenderingDeviceDriverMetal &p_driver, bool p_barriers) {
		p_driver.sync_mode = p_barriers ? RenderingDeviceDriverMetal::Barriers : RenderingDeviceDriverMetal::HazardTracking;
	}
};

TEST_CASE("[K03][MetalCompletion] native batches retire through typed and legacy owners") {
	using RDD = RenderingDeviceDriver;
	NS::SharedPtr<MTL::Device> available_device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE_MESSAGE(available_device.get() != nullptr, "Native Metal device unavailable; this is not a dummy-renderer control.");
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	INFO("Native Metal device: ", available_device->name()->utf8String());
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto queue = driver.command_queue_create(RDD::CommandQueueFamilyID(1));
	const auto pool = driver.command_pool_create(RDD::CommandQueueFamilyID(1), RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto buffer = driver.buffer_create(4, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	REQUIRE(bool(buffer));
	for (bool barriers : { false, true }) {
		TestMetalCompletionAccess::mode(driver, barriers);
		const auto command = driver.command_buffer_create(pool);
		const auto fence = driver.fence_create();
		REQUIRE(bool(fence));
		for (int attempt = 0; attempt < 2; attempt++) {
			REQUIRE(driver.command_buffer_begin(command));
			driver.command_clear_buffer(command, buffer, 0, 4);
			driver.command_buffer_end(command);
			REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
			const auto report = driver.get_last_queue_call_report();
			REQUIRE(report.issued);
			REQUIRE(report.submission_serial != 0);
			CHECK(TestMetalCompletionAccess::records(driver) > 0);
			if (attempt == 0) {
				const auto wrong = driver.fence_poll(fence, report.submission_serial + 1);
				CHECK_FALSE(wrong.matches(report.submission_serial + 1));
				CHECK(wrong.completed_serial == report.submission_serial);
				const auto completion = driver.fence_wait_observed(fence, report.submission_serial);
				REQUIRE(completion.matches(report.submission_serial));
				REQUIRE(driver.fence_recycle(fence, report.submission_serial) == OK);
				CHECK(driver.fence_recycle(fence, report.submission_serial) != OK);
			} else {
				REQUIRE(driver.fence_wait(fence) == OK);
				CHECK_FALSE(driver.fence_poll(fence, report.submission_serial).matches(report.submission_serial));
			}
			CHECK(TestMetalCompletionAccess::records(driver) == 0);
		}
		TestMetalCompletionAccess::exhaust(fence);
		REQUIRE(driver.command_buffer_begin(command));
		driver.command_clear_buffer(command, buffer, 0, 4);
		driver.command_buffer_end(command);
		CHECK(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == ERR_UNAVAILABLE);
		CHECK_FALSE(driver.get_last_queue_call_report().issued);
		CHECK(driver.get_last_queue_call_report().error == ERR_UNAVAILABLE);
		// Drain a baseline wrap without allowing it to masquerade as serial0.
		TestMetalCompletionAccess::drain(driver);
		driver.fence_free(fence);
	}
	driver.buffer_free(buffer);
	driver.command_pool_free(pool);
}

TEST_CASE("[K03][MetalCompletion] empty recordings submit a native fence before retirement") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto queue = driver.command_queue_create(RDD::CommandQueueFamilyID(1));
	const auto pool = driver.command_pool_create(RDD::CommandQueueFamilyID(1), RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	for (bool barriers : { false, true }) {
		TestMetalCompletionAccess::mode(driver, barriers);
		const auto fence = driver.fence_create();
		CHECK(driver.fence_wait(fence) == OK);
		uint64_t previous_serial = 0;
		for (int attempt = 0; attempt < 2; attempt++) {
			REQUIRE(driver.command_buffer_begin(command));
			driver.command_buffer_end(command);
			REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
			const auto report = driver.get_last_queue_call_report();
			CHECK(report.issued);
			REQUIRE(report.submission_serial > previous_serial);
			CHECK(TestMetalCompletionAccess::records(driver) > 0);
			const auto completion = driver.fence_wait_observed(fence, report.submission_serial);
			REQUIRE(completion.matches(report.submission_serial));
			CHECK(driver.fence_recycle(fence, report.submission_serial) == OK);
			CHECK_FALSE(driver.fence_poll(fence, report.submission_serial).matches(report.submission_serial));
			CHECK(TestMetalCompletionAccess::records(driver) == 0);
			previous_serial = report.submission_serial;
		}
		driver.fence_free(fence);
	}
	driver.command_pool_free(pool);
}

TEST_CASE("[K03][MetalCompletion] no fence batches do not accumulate unowned records") {
	using RDD = RenderingDeviceDriver;
	NS::SharedPtr<MTL::Device> available_device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE(available_device.get() != nullptr);
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto queue = driver.command_queue_create(RDD::CommandQueueFamilyID(1));
	const auto pool = driver.command_pool_create(RDD::CommandQueueFamilyID(1), RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	const auto buffer = driver.buffer_create(4, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	REQUIRE(bool(buffer));
	for (bool barriers : { false, true }) {
		TestMetalCompletionAccess::mode(driver, barriers);
		for (int attempt = 0; attempt < 8; attempt++) {
			REQUIRE(driver.command_buffer_begin(command));
			driver.command_clear_buffer(command, buffer, 0, 4);
			driver.command_buffer_end(command);
			REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, {}, {}) == OK);
			CHECK(TestMetalCompletionAccess::records(driver) == 0);
			for (const auto &fact : driver.get_last_queue_call_report().calls) {
				CHECK(fact.completion == RDD::CompletionCapability::NoCompletion);
			}
		}
	}
	// A final queue-ordered fence makes destruction of the shared buffer legitimate.
	const auto fence = driver.fence_create();
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_clear_buffer(command, buffer, 0, 4);
	driver.command_buffer_end(command);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
	REQUIRE(driver.fence_wait(fence) == OK);
	driver.fence_free(fence);
	driver.buffer_free(buffer);
	driver.command_pool_free(pool);
}

TEST_CASE("[K03][MetalCompletion] armed fence rejects overwrite before issuing another batch") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto queue = driver.command_queue_create(RDD::CommandQueueFamilyID(1));
	const auto pool = driver.command_pool_create(RDD::CommandQueueFamilyID(1), RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto buffer = driver.buffer_create(4, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	REQUIRE(bool(buffer));
	for (bool barriers : { false, true }) {
		TestMetalCompletionAccess::mode(driver, barriers);
		const auto fence = driver.fence_create();
		const auto first = driver.command_buffer_create(pool);
		const auto second = driver.command_buffer_create(pool);
		for (auto command : { first, second }) {
			REQUIRE(driver.command_buffer_begin(command));
			driver.command_clear_buffer(command, buffer, 0, 4);
			driver.command_buffer_end(command);
		}
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, first, {}, fence, {}) == OK);
		const auto original = driver.get_last_queue_call_report();
		const uint32_t original_records = TestMetalCompletionAccess::records(driver);
		const Error result = driver.command_queue_execute_and_present(queue, {}, second, {}, fence, {});
		const auto report = driver.get_last_queue_call_report();
		CHECK(result == ERR_BUSY);
		CHECK_FALSE(report.issued);
		CHECK(report.error == ERR_BUSY);
		CHECK(TestMetalCompletionAccess::records(driver) == original_records);
		CHECK(driver.fence_wait_observed(fence, original.submission_serial).matches(original.submission_serial));
		// On the failing baseline, retire the replacement so fixture teardown is safe.
		const uint64_t retire_serial = result == OK ? report.submission_serial : original.submission_serial;
		REQUIRE(driver.fence_wait_observed(fence, retire_serial).matches(retire_serial));
		REQUIRE(driver.fence_recycle(fence, retire_serial) == OK);
		driver.fence_free(fence);
	}
	driver.buffer_free(buffer);
	driver.command_pool_free(pool);
}

TEST_CASE("[K03][MetalCompletion] abandoned native records drain before resource teardown") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto queue = driver.command_queue_create(RDD::CommandQueueFamilyID(1));
	const auto pool = driver.command_pool_create(RDD::CommandQueueFamilyID(1), RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto command = driver.command_buffer_create(pool);
	const auto buffer = driver.buffer_create(4, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	REQUIRE(bool(buffer));
	const auto fence = driver.fence_create();
	REQUIRE(driver.command_buffer_begin(command));
	driver.command_clear_buffer(command, buffer, 0, 4);
	driver.command_buffer_end(command);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, {}, fence, {}) == OK);
	CHECK(TestMetalCompletionAccess::records(driver) > 0);
	// No caller observes/recycles this batch. The driver's global record owner
	// must wait for handlers and reclaim it while command wrappers still exist.
	driver.fence_free(fence);
	TestMetalCompletionAccess::drain(driver);
	CHECK(TestMetalCompletionAccess::records(driver) == 0);
	TestMetalCompletionAccess::drain(driver); // Base/derived destructor pairing.
	driver.buffer_free(buffer);
	driver.command_pool_free(pool);
}

TEST_CASE("[K03][MetalCompletion] separate native batch owners retain a terminal fault independently") {
	using RDD = RenderingDeviceDriver;
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 2) == OK);
	const auto queue = driver.command_queue_create(RDD::CommandQueueFamilyID(1));
	const auto pool = driver.command_pool_create(RDD::CommandQueueFamilyID(1), RDD::COMMAND_BUFFER_TYPE_PRIMARY);
	const auto buffer = driver.buffer_create(4, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	REQUIRE(bool(buffer));
	for (bool barriers : { false, true }) {
		TestMetalCompletionAccess::mode(driver, barriers);
		const auto first = driver.fence_create();
		const auto second = driver.fence_create();
		const auto semaphore = driver.semaphore_create();
		uint64_t serials[2] = {};
		int batch = 0;
		for (auto fence : { first, second }) {
			RDD::CommandBufferID commands[2];
			for (auto &command : commands) {
				command = driver.command_buffer_create(pool);
				REQUIRE(driver.command_buffer_begin(command));
				driver.command_clear_buffer(command, buffer, 0, 4);
				driver.command_buffer_end(command);
			}
			REQUIRE(driver.command_queue_execute_and_present(queue, batch == 0 ? VectorView<RDD::SemaphoreID>() : VectorView<RDD::SemaphoreID>(semaphore), VectorView<RDD::CommandBufferID>(commands, 2), semaphore, fence, {}) == OK);
			serials[batch++] = driver.get_last_queue_call_report().submission_serial;
		}
		REQUIRE(driver.fence_wait_observed(first, serials[0]).matches(serials[0]));
		REQUIRE(driver.fence_wait_observed(second, serials[1]).matches(serials[1]));
		const uint32_t all_records = TestMetalCompletionAccess::records(driver);
		CHECK(all_records >= 4);
		// Inject a terminal fault into an already terminal real native batch.
		// This verifies ownership/error routing, not an actual device fault.
		TestMetalCompletionAccess::fail_observed_batch(second, -703);
		const auto failure = driver.fence_poll(second, serials[1]);
		CHECK(failure.native_error);
		CHECK(failure.native_code == -703);
		CHECK(failure.completed_serial == serials[1]);
		CHECK_FALSE(failure.matches(serials[1]));
		REQUIRE(driver.fence_recycle(first, serials[0]) == OK);
		CHECK(TestMetalCompletionAccess::records(driver) < all_records);
		CHECK(TestMetalCompletionAccess::records(driver) > 0);
		const auto retained = driver.fence_poll(second, serials[1]);
		CHECK(retained.native_error);
		CHECK(retained.native_code == -703);
		CHECK(retained.completed_serial == serials[1]);
		driver.fence_free(first);
		driver.fence_free(second);
		driver.semaphore_free(semaphore);
		TestMetalCompletionAccess::drain(driver);
		CHECK(TestMetalCompletionAccess::records(driver) == 0);
	}
	driver.buffer_free(buffer);
	driver.command_pool_free(pool);
}
#endif
