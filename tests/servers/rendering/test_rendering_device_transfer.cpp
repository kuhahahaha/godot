/**************************************************************************/
/*  test_rendering_device_transfer.cpp                                    */
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

#include "core/os/os.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

TEST_FORCE_LINK(test_rendering_device_transfer)

class TestTransferWorkerAccess {
public:
	using Worker = RenderingDevice::TransferWorker;
	using Buffer = RenderingDevice::Buffer;
	using Texture = RenderingDevice::Texture;
	static void attach(RenderingDevice &p_rd, RenderingDeviceDriver *p_driver) {
		p_rd.driver = p_driver;
		p_rd.transfer_worker_pool.resize(4);
		p_rd.transfer_worker_operation_used_by_draw.resize(4);
		p_rd.transfer_worker_pool_max_size = 4;
	}
	static Worker *add(RenderingDevice &p_rd, bool p_submitted = true) {
		Worker *worker = memnew(Worker);
		worker->index = p_rd.transfer_worker_pool_size++;
		worker->staging_buffer = RenderingDeviceDriver::BufferID(101 + worker->index);
		worker->command_pool = RenderingDeviceDriver::CommandPoolID(201 + worker->index);
		worker->command_buffer = RenderingDeviceDriver::CommandBufferID(301 + worker->index);
		worker->command_fence = RenderingDeviceDriver::FenceID(401 + worker->index);
		worker->staging_buffer_size_allocated = 16384;
		worker->staging_buffer_size_in_use = 16;
		worker->submitted = p_submitted;
		worker->submission_serial = p_submitted ? 41 : 0;
		worker->operations_counter = 7;
		worker->operations_submitted = 7;
		worker->operations_processed = 3;
		p_rd.transfer_worker_pool[worker->index] = worker;
		p_rd.transfer_worker_pool_available_list.push_back(worker->index);
		return worker;
	}
	static bool poll(RenderingDevice &p_rd, Worker *p_worker) { return p_rd._wait_for_transfer_worker(p_worker); }
	static void submit(RenderingDevice &p_rd, Worker *p_worker) { p_rd._submit_transfer_worker(p_worker); }
	static Worker *acquire(RenderingDevice &p_rd, uint32_t p_size) {
		uint32_t offset = 0;
		return p_rd._acquire_transfer_worker(p_size, 1, offset);
	}
	static Worker *acquire_with_offset(RenderingDevice &p_rd, uint32_t p_size) {
		uint32_t offset = 0;
		return p_rd._acquire_transfer_worker(p_size, 1, offset);
	}
	static void release(RenderingDevice &p_rd, Worker *p_worker) { p_rd._release_transfer_worker(p_worker); }
	static Error sbt(RenderingDevice &p_rd) {
		Buffer buffer;
		return p_rd._raytracing_pipeline_create_sbt_buffer(RenderingDeviceDriver::RaytracingPipelineID(801), 1, 1, buffer);
	}
	static void free_pool(RenderingDevice &p_rd) { p_rd._free_transfer_workers(); }
	static uint32_t size(RenderingDevice &p_rd) { return p_rd.transfer_worker_pool_size; }
	static uint32_t available(RenderingDevice &p_rd) { return p_rd.transfer_worker_pool_available_list.size(); }
	static void limit_pool(RenderingDevice &p_rd) { p_rd.transfer_worker_pool_max_size = p_rd.transfer_worker_pool_size; }
	static void make_recording_full(Worker *p_worker) {
		p_worker->recording = true;
		p_worker->staging_buffer_size_in_use = p_worker->staging_buffer_size_allocated;
		p_worker->max_transfer_size = p_worker->staging_buffer_size_allocated * 2;
	}
	static void lock_pool(RenderingDevice &p_rd) { p_rd.transfer_worker_pool_mutex.lock(); }
	static void unlock_pool(RenderingDevice &p_rd) { p_rd.transfer_worker_pool_mutex.unlock(); }
	static void borrow(RenderingDevice &p_rd) { p_rd.transfer_worker_pool_available_list.clear(); }
	static Error initialize(RenderingDevice &p_rd, Buffer *p_buffer, Span<uint8_t> p_data) { return p_rd._buffer_initialize(p_buffer, p_data); }
	static RID texture(RenderingDevice &p_rd) {
		Texture texture;
		texture.driver_id = RenderingDeviceDriver::TextureID(701);
		texture.type = RenderingDevice::TEXTURE_TYPE_2D;
		texture.format = RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
		texture.width = texture.height = texture.depth = texture.layers = texture.mipmaps = 1;
		return p_rd.texture_owner.make_rid(texture);
	}
	static Error texture_initialize(RenderingDevice &p_rd, RID p_texture) {
		Vector<uint8_t> bytes;
		bytes.resize(4);
		return p_rd._texture_initialize(p_texture, 0, bytes, RenderingDeviceDriver::TEXTURE_LAYOUT_COPY_DST_OPTIMAL, true);
	}
	static void initialize_graph(RenderingDevice &p_rd, RenderingDeviceDriver *p_driver) {
		p_rd.draw_graph.initialize(p_driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RenderingDeviceDriver::CommandQueueFamilyID(1), 1);
	}
	static uint32_t texture_count(RenderingDevice &p_rd) { return p_rd.texture_owner.get_owned_list().size(); }
	struct StagingProbe {
		Error error;
		uint32_t offset;
		uint32_t size;
		uint64_t frame_used;
		uint32_t fill_amount;
		bool used;
	};
	static StagingProbe staging_probe(RenderingDevice &p_rd, uint32_t p_amount, uint64_t p_frame_used) {
		auto &staging = p_rd.upload_staging_buffers;
		staging.blocks.resize(1);
		staging.block_size = staging.max_size = 64;
		staging.blocks.write[0].frame_used = p_frame_used;
		staging.blocks.write[0].fill_amount = 0;
		p_rd.frames.resize(2);
		p_rd.frames_drawn = 10;
		uint32_t offset = 777, size = 888;
		RenderingDevice::StagingRequiredAction action = RenderingDevice::STAGING_REQUIRED_ACTION_NONE;
		Error error = p_rd._staging_buffer_allocate(staging, p_amount, 1, offset, size, action, false);
		StagingProbe result{ error, offset, size, staging.blocks[0].frame_used, staging.blocks[0].fill_amount, staging.used };
		staging.blocks.clear();
		staging.used = false;
		p_rd.frames.clear();
		return result;
	}
	static uint32_t normalized_region_size(int64_t p_size) {
		return RenderingDevice::_normalize_staging_region_size(p_size);
	}
	static void cleanup_created_texture(RenderingDevice &p_rd, RID p_rid) {
		p_rd.frames.resize(1);
		p_rd.render_thread_id = Thread::get_caller_id();
		p_rd.free_rid(p_rid);
		p_rd._free_pending_resources(0);
		p_rd.frames.clear();
	}
	static void detach(RenderingDevice &p_rd) {
		for (RID rid : p_rd.texture_owner.get_owned_list()) {
			p_rd.texture_owner.free(rid);
		}
		// The fake owns no GPU work. Discard its deliberately unresolved fixture
		// state without pretending production retirement has succeeded.
		for (uint32_t i = 0; i < p_rd.transfer_worker_pool_size; i++) {
			memdelete(p_rd.transfer_worker_pool[i]);
		}
		p_rd.transfer_worker_pool_size = 0;
		p_rd.transfer_worker_pool_available_list.clear();
		p_rd.driver = nullptr;
	}
};

namespace TestRenderingDeviceTransfer {
using RDD = RenderingDeviceDriver;
using Access = TestTransferWorkerAccess;

class Driver : public TransferTestDriverBase {
public:
	FencePollResult poll_result;
	FencePollResult wait_result;
	QueueCallReport issue_result;
	Error recycle_result = OK;
	bool reset_result = true;
	bool allocation_fails = false;
	bool map_fails = false;
	int polls = 0;
	int waits = 0;
	int recycles = 0;
	int resets = 0;
	int begins = 0;
	int copies = 0;
	int frees = 0;
	int fence_frees = 0;
	int pool_frees = 0;
	int texture_frees = 0;
	int texture_usage_queries = 0;
	int texture_creates = 0;
	bool reject_texture_format = false;
	bool finalize_sentinel = false;
	bool begin_result = true;
	int creation_failure = 0;
	int fence_creates = 0;
	int pool_creates = 0;
	int buffer_creates = 0;
	uint8_t data[32768] = {};
	Driver() {
		wait_result.completed = true;
		wait_result.completed_serial = 41;
		issue_result.issued = true;
		issue_result.submission_serial = 41;
	}
	FencePollResult fence_poll(FenceID, uint64_t p_serial) override {
		CHECK(p_serial == 41);
		polls++;
		return poll_result;
	}
	Error fence_wait(FenceID) override {
		CRASH_NOW_MSG("Legacy wait must not recycle a typed transfer fence.");
		std::abort();
	}
	FencePollResult fence_wait_observed(FenceID, uint64_t p_serial) override {
		CHECK(p_serial == 41);
		waits++;
		return wait_result;
	}
	Error fence_recycle(FenceID, uint64_t p_serial) override {
		CHECK(p_serial == 41);
		recycles++;
		return recycle_result;
	}
	bool command_pool_reset(CommandPoolID) override {
		resets++;
		return reset_result;
	}
	BufferID buffer_create(uint64_t, BitField<BufferUsageBits>, MemoryAllocationType, uint64_t) override { return allocation_fails ? BufferID() : BufferID(501); }
	void buffer_free(BufferID p_buffer) override {
		CHECK(p_buffer.id != 0);
		frees++;
	}
	void fence_free(FenceID) override { fence_frees++; }
	void command_pool_free(CommandPoolID) override {
		if (finalize_sentinel) {
			fprintf(stderr, "P2_DEPENDENT_FREE\n");
			fflush(stderr);
			std::_Exit(87);
		}
		pool_frees++;
	}
	FenceID fence_create() override {
		fence_creates++;
		return creation_failure == 1 ? FenceID() : FenceID(903);
	}
	CommandPoolID command_pool_create(CommandQueueFamilyID, CommandBufferType) override {
		pool_creates++;
		return creation_failure == 2 ? CommandPoolID() : CommandPoolID(901);
	}
	CommandBufferID command_buffer_create(CommandPoolID p_pool) override {
		CHECK(p_pool.id != 0);
		buffer_creates++;
		return creation_failure == 3 ? CommandBufferID() : CommandBufferID(902);
	}
	TextureID texture_create(const TextureFormat &, const TextureView &) override {
		texture_creates++;
		return TextureID(701);
	}
	void texture_free(TextureID) override { texture_frees++; }
	uint64_t texture_get_allocation_size(TextureID) override { return 4; }
	BitField<TextureUsageBits> texture_get_usages_supported_by_format(DataFormat, bool) override {
		texture_usage_queries++;
		return reject_texture_format ? BitField<TextureUsageBits>() : BitField<TextureUsageBits>(TEXTURE_USAGE_SAMPLING_BIT | TEXTURE_USAGE_VRS_ATTACHMENT_BIT);
	}
	uint64_t limit_get(Limit) override { return 4096; }
	uint8_t *buffer_map(BufferID) override { return map_fails ? nullptr : data; }
	void buffer_unmap(BufferID) override {}
	bool buffer_set_texel_format(BufferID, DataFormat) override { return true; }
	void set_object_name(ObjectType, ID, const String &) override {}
	uint64_t api_trait_get(ApiTrait p_trait) override {
		if (p_trait == API_TRAIT_HONORS_PIPELINE_BARRIERS) {
			return 0;
		}
		if (p_trait == API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT || p_trait == API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP) {
			return 1;
		}
		if (p_trait == API_TRAIT_SHADER_GROUP_HANDLE_SIZE || p_trait == API_TRAIT_SHADER_GROUP_HANDLE_ALIGNMENT || p_trait == API_TRAIT_SHADER_GROUP_BASE_ALIGNMENT) {
			return 4;
		}
		return 0;
	}
	bool raytracing_pipeline_get_shader_group_handles(RaytracingPipelineID, uint32_t, VectorView<uint32_t>, uint8_t *, uint32_t) override { return true; }
	void command_copy_buffer_to_texture(CommandBufferID, BufferID, TextureID, TextureLayout, VectorView<BufferTextureCopyRegion>) override { copies++; }
	bool command_buffer_begin(CommandBufferID) override {
		begins++;
		return begin_result;
	}
	void command_buffer_end(CommandBufferID) override {}
	void command_copy_buffer(CommandBufferID, BufferID, BufferID, VectorView<BufferCopyRegion>) override { copies++; }
	Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
		record_queue_call_report(issue_result);
		return issue_result.error;
	}
};

struct Fixture {
	Driver driver;
	RenderingDevice rd;
	Fixture() { Access::attach(rd, &driver); }
	~Fixture() { Access::detach(rd); }
};

TEST_CASE("[K03][P2] non-segmented staging rejects regions larger than its block before effects") {
	Fixture f;
	for (uint64_t frame_used : { uint64_t(0), uint64_t(10) }) {
		auto oversized = Access::staging_probe(f.rd, 65, frame_used);
		CHECK(oversized.error == ERR_INVALID_PARAMETER);
		CHECK(oversized.frame_used == frame_used);
		CHECK(oversized.fill_amount == 0);
		CHECK_FALSE(oversized.used);
	}
	auto exact = Access::staging_probe(f.rd, 64, 0);
	CHECK(exact.error == OK);
	CHECK(exact.offset == 0);
	CHECK(exact.size == 64);
}

TEST_CASE("[K03][P2] zero texture staging region normalizes before transfer loops") {
	CHECK(Access::normalized_region_size(0) == 1);
	CHECK(Access::normalized_region_size(-2) == 1);
	CHECK(Access::normalized_region_size(INT64_MIN) == 1);
	CHECK(Access::normalized_region_size(1) == 1);
	CHECK(Access::normalized_region_size(63) == 64);
	CHECK(Access::normalized_region_size(UINT32_MAX) == 1);
	CHECK(Access::normalized_region_size(int64_t(UINT32_MAX) + 1) == 1);
}

TEST_CASE("[K03][P2] transfer recycle Busy retains original work and permits later completion") {
	Fixture f;
	auto *worker = Access::add(f.rd);
	f.driver.poll_result.completed = true;
	f.driver.poll_result.completed_serial = 41;
	f.driver.recycle_result = ERR_BUSY;
	CHECK_FALSE(Access::poll(f.rd, worker));
	CHECK_FALSE(worker->poisoned);
	CHECK(worker->submitted);
	CHECK(worker->submission_serial == 41);
	CHECK(worker->staging_buffer_size_in_use == 16);
	CHECK(worker->operations_processed == 3);
	CHECK(f.driver.resets == 0);
	f.driver.recycle_result = OK;
	CHECK(Access::poll(f.rd, worker));
	CHECK_FALSE(worker->poisoned);
	CHECK_FALSE(worker->submitted);
	CHECK(worker->operations_processed == 7);
	CHECK(f.driver.recycles == 2);
	CHECK(f.driver.resets == 1);
}

TEST_CASE("[K03][P2] pending transfer retains its exact prefix and can complete later") {
	Fixture f;
	auto *worker = Access::add(f.rd);
	CHECK_FALSE(Access::poll(f.rd, worker));
	CHECK_FALSE(worker->poisoned);
	CHECK(worker->submitted);
	CHECK(worker->submission_serial == 41);
	CHECK(worker->staging_buffer_size_in_use == 16);
	CHECK(worker->operations_processed == 3);
	CHECK(f.driver.recycles == 0);
	CHECK(f.driver.resets == 0);
	CHECK(f.driver.waits == 0);
	f.driver.poll_result.completed = true;
	f.driver.poll_result.completed_serial = 41;
	CHECK(Access::poll(f.rd, worker));
	CHECK_FALSE(worker->submitted);
	CHECK(worker->submission_serial == 0);
	CHECK(worker->staging_buffer_size_in_use == 0);
	CHECK(worker->operations_processed == 7);
	CHECK(f.driver.recycles == 1);
	CHECK(f.driver.resets == 1);
}

TEST_CASE("[K03][P2] invalid completion never advances or reuses the transfer prefix") {
	for (int failure = 0; failure < 4; failure++) {
		Fixture f;
		auto *worker = Access::add(f.rd);
		f.driver.poll_result.completed = true;
		f.driver.poll_result.completed_serial = failure == 0 ? 40 : 41;
		f.driver.poll_result.native_error = failure == 1;
		f.driver.poll_result.native_code = failure == 1 ? -7 : 0;
		f.driver.recycle_result = failure == 2 ? FAILED : OK;
		f.driver.reset_result = failure != 3;
		CHECK_FALSE(Access::poll(f.rd, worker));
		CHECK(worker->poisoned);
		CHECK(worker->submitted);
		CHECK(worker->submission_serial == 41);
		CHECK(worker->staging_buffer_size_in_use == 16);
		CHECK(worker->operations_processed == 3);
		CHECK(worker->last_completion.completed_serial == f.driver.poll_result.completed_serial);
		CHECK(worker->last_completion.native_error == f.driver.poll_result.native_error);
		CHECK(worker->last_completion.native_code == f.driver.poll_result.native_code);
		Access::free_pool(f.rd);
		CHECK(Access::size(f.rd) == 1);
		CHECK(f.driver.frees == 0);
		CHECK(f.driver.fence_frees == 0);
		CHECK(f.driver.pool_frees == 0);
	}
}

TEST_CASE("[K03][P2] rejected and unknown transfer issue cannot become completed upload") {
	for (bool issued : { false, true }) {
		Fixture f;
		auto *worker = Access::add(f.rd, false);
		f.driver.issue_result.error = FAILED;
		f.driver.issue_result.issued = issued;
		f.driver.issue_result.submission_serial = 0;
		f.driver.issue_result.native_code = -811;
		f.driver.issue_result.present_attempted = true;
		RDD::NativeCallFact fact;
		fact.serial = 19;
		fact.issue = RDD::IssueFact::IssuedUnknown;
		fact.native_code = -812;
		fact.error = ERR_CANT_CREATE;
		fact.completion = RDD::CompletionCapability::CommandBuffer;
		fact.completion_serial = 23;
		fact.present = true;
		f.driver.issue_result.calls.push_back(fact);
		Access::submit(f.rd, worker);
		CHECK_FALSE(Access::poll(f.rd, worker));
		CHECK(worker->poisoned);
		CHECK(worker->last_submission.error == FAILED);
		CHECK(worker->last_submission.issued == issued);
		CHECK(worker->last_submission.submission_serial == 0);
		CHECK(worker->last_submission.native_code == -811);
		CHECK(worker->last_submission.present_attempted);
		REQUIRE(worker->last_submission.calls.size() == 1);
		const auto &retained = worker->last_submission.calls[0];
		CHECK(retained.serial == 19);
		CHECK(retained.issue == RDD::IssueFact::IssuedUnknown);
		CHECK(retained.native_code == -812);
		CHECK(retained.error == ERR_CANT_CREATE);
		CHECK(retained.completion == RDD::CompletionCapability::CommandBuffer);
		CHECK(retained.completion_serial == 23);
		CHECK(retained.present);
		CHECK(worker->operations_processed == 3);
		CHECK(f.driver.resets == 0);
		CHECK(f.driver.recycles == 0);
	}
}

TEST_CASE("[K03][P2] pool retry retains every slot until all workers complete") {
	Fixture f;
	auto *complete = Access::add(f.rd);
	auto *pending = Access::add(f.rd);
	f.driver.poll_result.completed = true;
	f.driver.poll_result.completed_serial = 41;
	REQUIRE(Access::poll(f.rd, complete));
	Access::free_pool(f.rd);
	CHECK(Access::size(f.rd) == 2);
	CHECK(f.driver.frees == 0);
	CHECK(f.driver.fence_frees == 0);
	CHECK(f.driver.pool_frees == 0);
	REQUIRE(Access::poll(f.rd, pending));
	Access::free_pool(f.rd);
	CHECK(Access::size(f.rd) == 0);
	CHECK(f.driver.frees == 2);
	CHECK(f.driver.fence_frees == 2);
	CHECK(f.driver.pool_frees == 2);
}

TEST_CASE("[K03][P2] aggregate poisoned available worker refuses acquisition at full capacity") {
	Fixture f;
	auto *worker = Access::add(f.rd);
	Access::limit_pool(f.rd);
	f.driver.poll_result.native_error = true;
	f.driver.poll_result.native_code = -703;
	CHECK_FALSE(Access::poll(f.rd, worker));
	CHECK(worker->last_completion.native_error);
	CHECK(worker->last_completion.native_code == -703);
	CHECK(Access::available(f.rd) == 1);
	auto *acquired = Access::acquire(f.rd, 4);
	CHECK(acquired == nullptr);
	if (acquired) {
		Access::release(f.rd, acquired);
	}
	CHECK(Access::size(f.rd) == 1);
	CHECK(f.driver.begins == 0);
	CHECK(f.driver.frees == 0);
	CHECK(f.driver.resets == 0);
}

#ifdef THREADS_ENABLED
TEST_CASE("[K03][P2] poisoned release synchronizes retirement with the pool wait mutex") {
	Fixture f;
	auto *worker = Access::add(f.rd, false);
	Access::limit_pool(f.rd);
	Access::borrow(f.rd);
	std::atomic<bool> released{ false };
	Access::lock_pool(f.rd);
	std::thread owner([&]() {
		worker->thread_mutex.lock();
		worker->poisoned = true;
		Access::release(f.rd, worker);
		released.store(true);
	});
	// Observe the worker unlock before testing that retirement cannot pass
	// the same pool mutex held across the waiter's predicate and wait entry.
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	bool worker_unlocked = false;
	while (std::chrono::steady_clock::now() < deadline) {
		if (worker->thread_mutex.try_lock()) {
			worker_unlocked = worker->poisoned;
			worker->thread_mutex.unlock();
			if (worker_unlocked) {
				break;
			}
		}
		std::this_thread::yield();
	}
	CHECK(worker_unlocked);
	const auto release_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
	while (!released.load() && std::chrono::steady_clock::now() < release_deadline) {
		std::this_thread::yield();
	}
	CHECK_FALSE(released.load());
	Access::unlock_pool(f.rd);
	owner.join();
	CHECK(released.load());
	CHECK(Access::available(f.rd) == 0);
	CHECK(Access::acquire(f.rd, 4) == nullptr);
}
#endif

TEST_CASE("[K03][P2] staging allocation failure releases lock and quarantines worker") {
	Fixture f;
	auto *worker = Access::add(f.rd, false);
	worker->staging_buffer_size_in_use = 0;
	f.driver.allocation_fails = true;
	auto *acquired = Access::acquire(f.rd, 32768);
	CHECK(acquired == nullptr);
	if (acquired != nullptr) {
		Access::release(f.rd, acquired); // Keep failing baseline teardown safe.
	}
	CHECK(worker->poisoned);
	CHECK(Access::available(f.rd) == 0);
	bool unlocked = worker->thread_mutex.try_lock();
	CHECK(unlocked);
	if (unlocked) {
		worker->thread_mutex.unlock();
	}
	CHECK(f.driver.begins == 0);
}

TEST_CASE("[K03][P2] buffer map failure releases worker without recording destination") {
	Fixture f;
	auto *worker = Access::add(f.rd, false);
	f.driver.map_fails = true;
	Access::Buffer buffer;
	buffer.driver_id = RDD::BufferID(601);
	uint8_t bytes[4] = { 1, 2, 3, 4 };
	CHECK(Access::initialize(f.rd, &buffer, bytes) == ERR_CANT_CREATE);
	CHECK(buffer.transfer_worker_index == -1);
	CHECK(f.driver.copies == 0);
	bool unlocked = worker->thread_mutex.try_lock();
	CHECK(unlocked);
	if (unlocked) {
		worker->thread_mutex.unlock();
	}
}

TEST_CASE("[K03][P2] synchronous acquisition waits once and recycles exactly once") {
	Fixture f;
	auto *worker = Access::add(f.rd);
	auto *acquired = Access::acquire(f.rd, 4);
	CHECK(acquired == worker);
	if (acquired != nullptr) {
		Access::release(f.rd, acquired);
	}
	CHECK(f.driver.waits == 1);
	CHECK(f.driver.polls == 0);
	CHECK(f.driver.recycles == 1);
	CHECK(f.driver.resets == 1);
	CHECK(worker->operations_processed == 7);
	CHECK(f.driver.begins == 1);
}

TEST_CASE("[K03][P2] recording staging growth propagates its checked wait failure") {
	for (int mode = 0; mode < 7; mode++) {
		Fixture f;
		auto *worker = Access::add(f.rd, false);
		TestTransferWorkerAccess::make_recording_full(worker);
		if (mode == 0) { // Pending.
			f.driver.wait_result.completed = false;
		} else if (mode == 1) { // Matching completion.
			f.driver.wait_result.completed = true;
		} else if (mode == 2) { // Wrong serial.
			f.driver.wait_result.completed = true;
			f.driver.wait_result.completed_serial = 40;
		} else if (mode == 3) { // Native error.
			f.driver.wait_result.native_error = true;
			f.driver.wait_result.native_code = -905;
		} else if (mode == 4) { // Recycle failure.
			f.driver.wait_result.completed = true;
			f.driver.recycle_result = FAILED;
		} else if (mode == 5) { // Reset failure.
			f.driver.wait_result.completed = true;
			f.driver.reset_result = false;
		} else { // Rejected/issued-unknown issue.
			f.driver.issue_result.error = FAILED;
			f.driver.issue_result.issued = false;
			f.driver.issue_result.submission_serial = 0;
		}
		auto *acquired = Access::acquire_with_offset(f.rd, 4);
		if (mode == 1) {
			CHECK(acquired == worker);
			CHECK_FALSE(worker->poisoned);
			CHECK_FALSE(worker->submitted);
			CHECK(f.driver.waits == 1);
			CHECK(f.driver.recycles == 1);
			CHECK(f.driver.resets == 1);
			if (acquired != nullptr) {
				Access::release(f.rd, acquired);
			}
		} else {
			CHECK(acquired == nullptr);
			CHECK(worker->recording == false);
			if (mode == 0) {
				CHECK_FALSE(worker->poisoned);
				CHECK(worker->submitted);
			} else {
				CHECK(worker->poisoned);
			}
			if (mode != 6) {
				CHECK(worker->submission_serial == 41);
				CHECK(f.driver.waits == 1);
			}
			CHECK(f.driver.recycles == ((mode == 4 || mode == 5) ? 1 : 0));
			CHECK(f.driver.resets == (mode == 5 ? 1 : 0));
		}
	}
}

TEST_CASE("[K03][P2] immediate texture initialization observes completion before success") {
	Fixture f;
	auto *worker = Access::add(f.rd, false);
	RID texture = Access::texture(f.rd);
	CHECK(Access::texture_initialize(f.rd, texture) == OK);
	CHECK(f.driver.copies == 1);
	CHECK(f.driver.waits == 1);
	CHECK(f.driver.polls == 0);
	CHECK(f.driver.recycles == 1);
	CHECK_FALSE(worker->submitted);
	CHECK(worker->operations_processed == 8);
}

TEST_CASE("[K03][P2] ordinary buffer and SBT callers clean rejected acquisition allocations") {
	for (int caller = 0; caller < 6; caller++) {
		Fixture f;
		Access::add(f.rd);
		f.driver.wait_result.native_error = true;
		f.driver.wait_result.native_code = -7;
		uint8_t bytes[4] = {};
		ERR_PRINT_OFF;
		switch (caller) {
			case 0:
				CHECK(f.rd.storage_buffer_create(4, bytes).is_null());
				break;
			case 1:
				CHECK(f.rd.texture_buffer_create(1, RDD::DATA_FORMAT_R32_SFLOAT, bytes).is_null());
				break;
			case 2:
				CHECK(f.rd.vertex_buffer_create(4, bytes, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT).is_null());
				break;
			case 3:
				CHECK(f.rd.index_buffer_create(2, RDD::INDEX_BUFFER_FORMAT_UINT16, bytes).is_null());
				break;
			case 4:
				CHECK(f.rd.uniform_buffer_create(4, bytes).is_null());
				break;
			case 5:
				CHECK(Access::sbt(f.rd) != OK);
				break;
		}
		ERR_PRINT_ON;
		CHECK(f.driver.copies == 0);
		CHECK(f.driver.frees == 1);
		CHECK(f.driver.resets == 0);
		CHECK(f.driver.recycles == 0);
	}
}

TEST_CASE("[K03][P2] rejected buffer acquisition releases its resource tracker") {
	for (bool vertex : { false, true }) {
		Fixture f;
		Access::add(f.rd);
		Access::limit_pool(f.rd);
		f.driver.wait_result.native_error = true;
		uint8_t bytes[4] = {};
		ERR_PRINT_OFF;
		for (int attempt = 0; attempt < 2; attempt++) {
			uint64_t before = Memory::get_mem_usage();
			RID result = vertex ? f.rd.vertex_buffer_create(4, bytes, RenderingDevice::BUFFER_CREATION_AS_STORAGE_BIT) : f.rd.storage_buffer_create(4, bytes);
			CHECK(result.is_null());
			if (attempt == 1) {
				CHECK(Memory::get_mem_usage() == before);
			}
		}
		ERR_PRINT_ON;
	}
}

TEST_CASE("[K03][P2] failed command begin never lends a worker for upload") {
	Fixture f;
	auto *worker = Access::add(f.rd, false);
	f.driver.begin_result = false;
	auto *acquired = Access::acquire(f.rd, 4);
	CHECK(acquired == nullptr);
	if (acquired) {
		Access::release(f.rd, acquired);
	}
	CHECK(worker->poisoned);
	CHECK_FALSE(worker->recording);
}

TEST_CASE("[K03][P2] native worker creation failure rolls back only unissued ownership") {
	for (int failure = 1; failure <= 3; failure++) {
		Fixture f;
		f.driver.creation_failure = failure;
		auto *worker = Access::acquire(f.rd, 4);
		CHECK(worker == nullptr);
		if (worker) {
			Access::release(f.rd, worker);
		}
		CHECK(Access::size(f.rd) == 0);
		CHECK(Access::available(f.rd) == 0);
		CHECK(f.driver.begins == 0);
		CHECK(f.driver.pool_creates == (failure > 1 ? 1 : 0));
		CHECK(f.driver.buffer_creates == (failure > 2 ? 1 : 0));
		CHECK(f.driver.fence_frees == (failure > 1 ? 1 : 0));
		CHECK(f.driver.pool_frees == (failure > 2 ? 1 : 0));
	}
}

TEST_CASE("[K03][P2] texture creation rejects invalid base format before backend queries") {
	Fixture f;
	f.driver.reject_texture_format = true;
	RenderingDevice::TextureFormat format;
	format.format = RDD::DATA_FORMAT_MAX;
	format.width = format.height = format.depth = format.array_layers = format.mipmaps = 1;
	format.usage_bits = RDD::TEXTURE_USAGE_SAMPLING_BIT;
	ERR_PRINT_OFF;
	RID invalid = f.rd.texture_create(format, RenderingDevice::TextureView());
	ERR_PRINT_ON;
	CHECK(invalid.is_null());
	CHECK(f.driver.texture_usage_queries == 0);
	CHECK(Access::texture_count(f.rd) == 0);
	Vector<uint8_t> bytes;
	bytes.resize(4);
	Vector<Vector<uint8_t>> data;
	data.push_back(bytes);
	ERR_PRINT_OFF;
	invalid = f.rd.texture_create(format, RenderingDevice::TextureView(), data);
	format.usage_bits = RDD::TEXTURE_USAGE_VRS_ATTACHMENT_BIT;
	RID invalid_vrs = f.rd.texture_create(format, RenderingDevice::TextureView());
	ERR_PRINT_ON;
	CHECK(invalid.is_null());
	CHECK(invalid_vrs.is_null());
	CHECK(f.driver.texture_usage_queries == 0);
	CHECK(f.driver.texture_creates == 0);
	format.usage_bits = RDD::TEXTURE_USAGE_SAMPLING_BIT;
	// Check malformed enum storage without converting it to an invalid C++ enum.
	const uint32_t malformed = UINT32_MAX;
	static_assert(sizeof(format.format) == sizeof(malformed));
	memcpy(&format.format, &malformed, sizeof(malformed));
	ERR_PRINT_OFF;
	invalid = f.rd.texture_create(format, RenderingDevice::TextureView());
	ERR_PRINT_ON;
	CHECK(invalid.is_null());
	CHECK(f.driver.texture_usage_queries == 0);
	CHECK(Access::texture_count(f.rd) == 0);
	// A valid format still reaches the ordinary backend capability refusal.
	format.format = RDD::DATA_FORMAT_R8G8B8A8_UNORM;
	ERR_PRINT_OFF;
	RID unsupported = f.rd.texture_create(format, RenderingDevice::TextureView());
	ERR_PRINT_ON;
	CHECK(unsupported.is_null());
	CHECK(f.driver.texture_usage_queries == 1);
	CHECK(Access::texture_count(f.rd) == 0);
	f.driver.reject_texture_format = false;
	format.format = RDD::DATA_FORMAT_R32G32B32A32_SFLOAT;
	RID valid = f.rd.texture_create(format, RenderingDevice::TextureView());
	CHECK(valid.is_valid());
	CHECK(f.driver.texture_creates == 1);
	CHECK(Access::texture_count(f.rd) == 1);
	if (valid.is_valid()) {
		Access::cleanup_created_texture(f.rd, valid);
	}
	CHECK(f.driver.texture_frees == 1);
	CHECK(Access::texture_count(f.rd) == 0);
}

TEST_CASE("[K03][P2] public immediate texture creation propagates unresolved completion") {
	Fixture f;
	auto *worker = Access::add(f.rd, false);
	f.driver.wait_result.native_error = true;
	f.driver.wait_result.native_code = -7;
	RenderingDevice::TextureFormat format;
	format.format = RDD::DATA_FORMAT_R8G8B8A8_UNORM;
	format.width = format.height = format.depth = format.array_layers = format.mipmaps = 1;
	format.usage_bits = RDD::TEXTURE_USAGE_VRS_ATTACHMENT_BIT;
	ERR_PRINT_OFF;
	RID result = f.rd.texture_create(format, RenderingDevice::TextureView());
	ERR_PRINT_ON;
	CHECK(result.is_null());
	CHECK(worker->poisoned);
	CHECK(worker->submitted);
	CHECK(f.driver.copies == 1);
	CHECK(f.driver.texture_frees == 0);
	CHECK(Access::texture_count(f.rd) == 1); // Failed destination remains owned until safe shutdown.
}

TEST_CASE("[K03][P2] destructive finalize child") {
	if (!OS::get_singleton()->has_environment("GODOT_P2_FINALIZE_CHILD")) {
		return;
	}
	const String mode = OS::get_singleton()->get_environment("GODOT_P2_FINALIZE_CHILD");
	Fixture f;
	auto *worker = Access::add(f.rd);
	Access::initialize_graph(f.rd, &f.driver);
	f.driver.finalize_sentinel = true;
	if (mode == "pending") {
		f.driver.wait_result.completed = false;
	}
	if (mode == "error") {
		f.driver.wait_result.native_error = true;
		f.driver.wait_result.native_code = -7;
	}
	if (mode == "poisoned") {
		worker->poisoned = true;
	}
	fprintf(stderr, "P2_BEFORE_FINALIZE\n");
	fflush(stderr);
	f.rd.finalize();
	fprintf(stderr, "P2_AFTER_FINALIZE\n");
	fflush(stderr);
	std::_Exit(88);
}
} // namespace TestRenderingDeviceTransfer
