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
	static bool has_queue_order_event(const MTL3::RenderingDeviceDriverMetal &p_driver) {
		return p_driver.queue_order_event.get() != nullptr;
	}
	static uint64_t committed_order_value(const MTL3::RenderingDeviceDriverMetal &p_driver) {
		return p_driver.committed_order_value.load(std::memory_order_acquire);
	}
	static uint64_t command_wait_value(RenderingDeviceDriver::CommandBufferID p_cmd) {
		return reinterpret_cast<const MTL3::MDCommandBuffer *>(p_cmd.id)->get_queue_order_wait_value();
	}
	static bool is_untracked_buffer(RenderingDeviceDriver::BufferID p_buffer) {
		const auto *info = reinterpret_cast<const RenderingDeviceDriverMetal::BufferInfo *>(p_buffer.id);
		return info != nullptr && info->buffer.get() != nullptr && (info->buffer->resourceOptions() & MTL::ResourceHazardTrackingModeUntracked) != 0;
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
		CHECK(driver.qualification_native_call_bound(1740) == 0);
		CHECK(driver.qualification_native_call_bound_for_profile(145, 1595, 145) == 3770);
		CHECK(driver.qualification_native_call_bound_for_profile(145, 1758, 145) == 4096);
		CHECK(driver.qualification_native_call_bound_for_profile(145, 1759, 145) == 0);
		CHECK(driver.qualification_native_call_bound_for_profile(0, 0, 0) == 0);
		CHECK(driver.qualification_native_call_bound_for_profile(UINT32_MAX, 1, 1) == 0);
		const auto signal = driver.semaphore_create();
		const auto command = driver.command_buffer_create(pool);
		const auto fence = driver.fence_create();
		REQUIRE(bool(fence));
		for (int attempt = 0; attempt < 2; attempt++) {
			REQUIRE(driver.command_buffer_begin(command));
			driver.command_clear_buffer(command, buffer, 0, 4);
			driver.command_buffer_end(command);
			REQUIRE(driver.command_queue_execute_and_present(queue, {}, command, signal, fence, {}) == OK);
			const auto report = driver.get_last_queue_call_report();
			REQUIRE(report.issued);
			REQUIRE(report.submission_serial != 0);
			CHECK(report.calls.size() == (barriers ? 2 : 1)); // Actual waitless worker call shape.
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
		driver.semaphore_free(signal);
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

TEST_CASE("[K03][MetalCompletion] no-fence native calls recycle after completion") {
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
			const auto report = driver.get_last_queue_call_report();
			REQUIRE(report.issued);
			REQUIRE(report.submission_serial == 0);
			REQUIRE_FALSE(report.calls.is_empty());
			CHECK(TestMetalCompletionAccess::records(driver) == report.calls.size());
			for (const auto &fact : report.calls) {
				REQUIRE(fact.serial != 0);
				CHECK(fact.completion == RDD::CompletionCapability::CommandBuffer);
				CHECK(fact.completion_serial == fact.serial);
				REQUIRE(driver.native_call_wait(fact.serial).matches(fact.serial));
				CHECK(driver.native_call_recycle(fact.serial) == OK);
				CHECK(driver.native_call_recycle(fact.serial) == ERR_DOES_NOT_EXIST);
			}
			CHECK(TestMetalCompletionAccess::records(driver) == 0);
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

TEST_CASE("[K03][MetalCompletion][BUG-211] queue-order event serializes untracked command buffers without CPU stalls") {
	using RDD = RenderingDeviceDriver;
	NS::SharedPtr<MTL::Device> available_device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
	REQUIRE_MESSAGE(available_device.get() != nullptr, "Native Metal device unavailable; this is not a dummy-renderer control.");
	RenderingContextDriverMetal context;
	REQUIRE(context.initialize() == OK);
	MTL3::RenderingDeviceDriverMetal driver(&context);
	REQUIRE(driver.initialize(0, 3) == OK);
	TestMetalCompletionAccess::mode(driver, true);
	REQUIRE(TestMetalCompletionAccess::has_queue_order_event(driver));
	CHECK(TestMetalCompletionAccess::committed_order_value(driver) == 0);

	const auto queue = driver.command_queue_create(RDD::CommandQueueFamilyID(1));
	const auto pool = driver.command_pool_create(RDD::CommandQueueFamilyID(1), RDD::COMMAND_BUFFER_TYPE_PRIMARY);

	// 1. Empty command buffers (with and without fence) must not advance committed_order_value.
	const auto empty_cmd = driver.command_buffer_create(pool);
	REQUIRE(driver.command_buffer_begin(empty_cmd));
	driver.command_buffer_end(empty_cmd);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, empty_cmd, {}, {}, {}) == OK);
	CHECK(TestMetalCompletionAccess::committed_order_value(driver) == 0);
	{
		const auto report = driver.get_last_queue_call_report();
		for (const auto &fact : report.calls) {
			REQUIRE(driver.native_call_wait(fact.serial).matches(fact.serial));
			REQUIRE(driver.native_call_recycle(fact.serial) == OK);
		}
	}

	const auto empty_fence = driver.fence_create();
	REQUIRE(driver.command_buffer_begin(empty_cmd));
	driver.command_buffer_end(empty_cmd);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, empty_cmd, {}, empty_fence, {}) == OK);
	CHECK(TestMetalCompletionAccess::committed_order_value(driver) == 0);
	REQUIRE(driver.fence_wait(empty_fence) == OK);
	driver.fence_free(empty_fence);

	// 2. HazardTracking mode must not advance committed_order_value.
	const auto small_buf = driver.buffer_create(64, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	REQUIRE(bool(small_buf));
	TestMetalCompletionAccess::mode(driver, false);
	{
		const auto ht_fence = driver.fence_create();
		REQUIRE(driver.command_buffer_begin(empty_cmd));
		driver.command_clear_buffer(empty_cmd, small_buf, 0, 64);
		driver.command_buffer_end(empty_cmd);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, empty_cmd, {}, ht_fence, {}) == OK);
		CHECK(TestMetalCompletionAccess::committed_order_value(driver) == 0);
		REQUIRE(driver.fence_wait(ht_fence) == OK);
		driver.fence_free(ht_fence);
	}
	TestMetalCompletionAccess::mode(driver, true);

	// 3. Rejected submission (exhausted fence) must not advance committed_order_value.
	{
		const auto rejected_cmd = driver.command_buffer_create(pool);
		const auto exhausted_fence = driver.fence_create();
		TestMetalCompletionAccess::exhaust(exhausted_fence);
		REQUIRE(driver.command_buffer_begin(rejected_cmd));
		driver.command_clear_buffer(rejected_cmd, small_buf, 0, 64);
		driver.command_buffer_end(rejected_cmd);
		CHECK(driver.command_queue_execute_and_present(queue, {}, rejected_cmd, {}, exhausted_fence, {}) == ERR_UNAVAILABLE);
		CHECK(TestMetalCompletionAccess::committed_order_value(driver) == 0);
		driver.fence_free(exhausted_fence);
	}

	// 4. Non-empty no-fence submission in Barriers mode advances committed_order_value.
	REQUIRE(driver.command_buffer_begin(empty_cmd));
	driver.command_clear_buffer(empty_cmd, small_buf, 0, 64);
	driver.command_buffer_end(empty_cmd);
	REQUIRE(driver.command_queue_execute_and_present(queue, {}, empty_cmd, {}, {}, {}) == OK);
	CHECK(TestMetalCompletionAccess::committed_order_value(driver) == 1);
	{
		const auto report = driver.get_last_queue_call_report();
		for (const auto &fact : report.calls) {
			REQUIRE(driver.native_call_wait(fact.serial).matches(fact.serial));
			REQUIRE(driver.native_call_recycle(fact.serial) == OK);
		}
	}
	driver.buffer_free(small_buf);

	// 5. Three distinct command buffers writing, reading, and clearing the same Untracked GPU
	//    storage buffer back-to-back with zero CPU waits between submissions (frame_queue_size=3 shape).
	constexpr uint32_t WORD_COUNT = 65536;
	constexpr uint64_t BUFFER_BYTES = uint64_t(WORD_COUNT) * sizeof(uint32_t);
	const auto src_buf = driver.buffer_create(BUFFER_BYTES, RDD::BUFFER_USAGE_TRANSFER_FROM_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	const auto untracked_gpu_buf = driver.buffer_create(BUFFER_BYTES, RDD::BUFFER_USAGE_STORAGE_BIT | RDD::BUFFER_USAGE_TRANSFER_FROM_BIT | RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_GPU, 0);
	const auto readback_buf = driver.buffer_create(BUFFER_BYTES, RDD::BUFFER_USAGE_TRANSFER_TO_BIT, RDD::MEMORY_ALLOCATION_TYPE_CPU, 0);
	REQUIRE(bool(src_buf));
	REQUIRE(bool(untracked_gpu_buf));
	REQUIRE(bool(readback_buf));
	REQUIRE(TestMetalCompletionAccess::is_untracked_buffer(untracked_gpu_buf));

	const auto cb1 = driver.command_buffer_create(pool);
	const auto cb2 = driver.command_buffer_create(pool);
	const auto cb3 = driver.command_buffer_create(pool);
	const auto f1 = driver.fence_create();
	const auto f2 = driver.fence_create();
	const auto f3 = driver.fence_create();
	const RDD::BufferCopyRegion full_copy = { 0, 0, BUFFER_BYTES };

	for (uint32_t iter = 0; iter < 16; ++iter) {
		uint32_t *src_words = reinterpret_cast<uint32_t *>(driver.buffer_map(src_buf));
		uint32_t *dst_words = reinterpret_cast<uint32_t *>(driver.buffer_map(readback_buf));
		REQUIRE(src_words != nullptr);
		REQUIRE(dst_words != nullptr);
		for (uint32_t w = 0; w < WORD_COUNT; ++w) {
			src_words[w] = 0x5A000000u | ((iter & 0xFFu) << 16) | (w & 0xFFFFu);
			dst_words[w] = 0xDEADBEEFu;
		}
		driver.buffer_unmap(src_buf);
		driver.buffer_unmap(readback_buf);

		const uint64_t base_order = TestMetalCompletionAccess::committed_order_value(driver);

		// CB1: write pattern into Untracked GPU buffer, submit without waiting.
		REQUIRE(driver.command_buffer_begin(cb1));
		driver.command_copy_buffer(cb1, src_buf, untracked_gpu_buf, full_copy);
		CHECK(TestMetalCompletionAccess::command_wait_value(cb1) == base_order);
		driver.command_buffer_end(cb1);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, cb1, {}, f1, {}) == OK);
		CHECK(TestMetalCompletionAccess::committed_order_value(driver) == base_order + 1);

		// CB2: read Untracked GPU buffer into readback buffer, submit without waiting.
		REQUIRE(driver.command_buffer_begin(cb2));
		driver.command_copy_buffer(cb2, untracked_gpu_buf, readback_buf, full_copy);
		CHECK(TestMetalCompletionAccess::command_wait_value(cb2) == base_order + 1);
		driver.command_buffer_end(cb2);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, cb2, {}, f2, {}) == OK);
		CHECK(TestMetalCompletionAccess::committed_order_value(driver) == base_order + 2);

		// CB3: immediately clear Untracked GPU buffer to 0, submit without waiting on CB1/CB2.
		REQUIRE(driver.command_buffer_begin(cb3));
		driver.command_clear_buffer(cb3, untracked_gpu_buf, 0, BUFFER_BYTES);
		CHECK(TestMetalCompletionAccess::command_wait_value(cb3) == base_order + 2);
		driver.command_buffer_end(cb3);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, cb3, {}, f3, {}) == OK);
		CHECK(TestMetalCompletionAccess::committed_order_value(driver) == base_order + 3);

		// Now wait on all three fences and verify CB2 observed exact CB1 data (no race with CB1 or CB3).
		REQUIRE(driver.fence_wait(f3) == OK);
		REQUIRE(driver.fence_wait(f2) == OK);
		REQUIRE(driver.fence_wait(f1) == OK);
		CHECK(TestMetalCompletionAccess::records(driver) == 0);

		const uint32_t *verify_words = reinterpret_cast<const uint32_t *>(driver.buffer_map(readback_buf));
		REQUIRE(verify_words != nullptr);
		uint32_t mismatches = 0;
		for (uint32_t w = 0; w < WORD_COUNT; ++w) {
			const uint32_t expected = 0x5A000000u | ((iter & 0xFFu) << 16) | (w & 0xFFFFu);
			if (verify_words[w] != expected) {
				++mismatches;
			}
		}
		driver.buffer_unmap(readback_buf);
		REQUIRE(mismatches == 0);
	}

	// 6. Pre-recorded multi-CB batch (commands.size() == 2) maintains monotonic queue order without deadlock.
	{
		const uint64_t before_multi = TestMetalCompletionAccess::committed_order_value(driver);
		RDD::CommandBufferID multi[2] = { cb1, cb2 };
		for (auto cmd : multi) {
			REQUIRE(driver.command_buffer_begin(cmd));
			driver.command_clear_buffer(cmd, untracked_gpu_buf, 0, BUFFER_BYTES);
			CHECK(TestMetalCompletionAccess::command_wait_value(cmd) == before_multi);
			driver.command_buffer_end(cmd);
		}
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, VectorView<RDD::CommandBufferID>(multi, 2), {}, f1, {}) == OK);
		CHECK(TestMetalCompletionAccess::committed_order_value(driver) == before_multi + 2);

		REQUIRE(driver.command_buffer_begin(cb3));
		driver.command_clear_buffer(cb3, untracked_gpu_buf, 0, BUFFER_BYTES);
		CHECK(TestMetalCompletionAccess::command_wait_value(cb3) == before_multi + 2);
		driver.command_buffer_end(cb3);
		REQUIRE(driver.command_queue_execute_and_present(queue, {}, cb3, {}, f2, {}) == OK);
		CHECK(TestMetalCompletionAccess::committed_order_value(driver) == before_multi + 3);

		REQUIRE(driver.fence_wait(f2) == OK);
		REQUIRE(driver.fence_wait(f1) == OK);
		CHECK(TestMetalCompletionAccess::records(driver) == 0);
	}

	driver.fence_free(f3);
	driver.fence_free(f2);
	driver.fence_free(f1);
	driver.buffer_free(readback_buf);
	driver.buffer_free(untracked_gpu_buf);
	driver.buffer_free(src_buf);
	driver.command_pool_free(pool);
}
#endif
