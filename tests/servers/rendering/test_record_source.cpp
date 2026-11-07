/**************************************************************************/
/*  test_record_source.cpp                                                */
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

#include "core/extension/gdextension_qualification_internal.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <type_traits>
TEST_FORCE_LINK(test_record_source)

// Controlled native observations exercise original source machinery. They do
// not issue production scope/record authority or certify a physical GPU path.
class TestRecordSourceAccess {
	using Origin = GDExtensionDeviceExit::NativeSourceOrigin;
	using Member = GDExtensionDeviceExit::NativeSourceMember;
	using Observation = GDExtensionDeviceExit::NativeSourceObservation;
	struct Driver : TransferTestDriverBase {
		uint32_t calls = 0, retains = 0, releases = 0, polls = 0, waits = 0;
		Error retain_error = OK, release_error = OK;
		FencePollResult observed;
		bool unknown_issue = false;
		bool honors_barriers = true;
		uint32_t expected_uniform_set = 51;
		uint32_t push_calls = 0, prepare_calls = 0, set_calls = 0, dispatch_calls = 0;
		uint32_t pushed_words[4] = {};
		CommandBufferID command_buffer_create(CommandPoolID) override { return CommandBufferID(61); }
		bool command_buffer_begin(CommandBufferID) override { return true; }
		void command_buffer_end(CommandBufferID) override {}
		SemaphoreID semaphore_create() override { return SemaphoreID(62); }
		void command_insert_breadcrumb(CommandBufferID, uint32_t) override {}
		void command_pipeline_barrier(CommandBufferID, BitField<PipelineStageBits>, BitField<PipelineStageBits>, VectorView<MemoryAccessBarrier>, VectorView<BufferBarrier>, VectorView<TextureBarrier>, VectorView<AccelerationStructureBarrier>) override {}
		void command_bind_compute_pipeline(CommandBufferID, PipelineID p_pipeline) override { CHECK(p_pipeline == PipelineID(8)); }
		void command_bind_push_constants(CommandBufferID, ShaderID p_shader, uint32_t p_first, VectorView<uint32_t> p_data) override {
			CHECK(p_shader == ShaderID(7));
			CHECK(p_first == 0);
			REQUIRE(p_data.size() == 4);
			++push_calls;
			for (uint32_t i = 0; i < 4; ++i) {
				pushed_words[i] = p_data[i];
			}
		}
		void command_uniform_set_prepare_for_use(CommandBufferID, UniformSetID p_set, ShaderID p_shader, uint32_t p_index) override {
			CHECK(p_set == UniformSetID(9));
			CHECK(p_shader == ShaderID(7));
			CHECK(p_index == 0);
			++prepare_calls;
		}
		void command_bind_compute_uniform_sets(CommandBufferID, VectorView<UniformSetID> p_sets, ShaderID p_shader, uint32_t p_first, uint32_t p_count, uint32_t) override {
			CHECK(p_shader == ShaderID(7));
			CHECK(p_first == 0);
			CHECK(p_count == 1);
			REQUIRE(p_sets.size() == 1);
			CHECK(p_sets[0] == UniformSetID(9));
			++set_calls;
		}
		void command_compute_dispatch(CommandBufferID, uint32_t p_x, uint32_t p_y, uint32_t p_z) override {
			CHECK(p_x == 1);
			CHECK(p_y == 2);
			CHECK(p_z == 3);
			++dispatch_calls;
		}

		uint32_t record_native_call_bound(uint32_t slots) const override { return slots; }
		bool native_call_observation_supported() const override { return true; }
		Error native_call_retain(uint64_t serial) override {
			CHECK(serial == 17);
			++retains;
			return retain_error;
		}
		Error native_call_release(uint64_t serial) override {
			CHECK(serial == 17);
			++releases;
			return release_error;
		}
		FencePollResult native_call_poll(uint64_t serial) override {
			CHECK(serial == 17);
			++polls;
			return observed;
		}
		FencePollResult native_call_wait(uint64_t) override {
			++waits;
			return {};
		}
		uint64_t limit_get(Limit) override { return 64; }
		uint64_t api_trait_get(ApiTrait trait) override { return trait == API_TRAIT_HONORS_PIPELINE_BARRIERS && honors_barriers ? 1 : 0; }
		uint32_t uniform_sets_get_dynamic_offsets(VectorView<UniformSetID> sets, ShaderID, uint32_t first, uint32_t count) const override {
			CHECK(sets.size() == 1);
			CHECK(sets[0] == UniformSetID(expected_uniform_set));
			CHECK(first == 0);
			CHECK(count == 1);
			return 0; // The controlled set has no dynamic descriptors.
		}
		Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID> swaps) override {
			CHECK(swaps.size() == 0);
			uint32_t index = UINT32_MAX;
			if (!claim_native_fact(index)) {
				return ERR_PARAMETER_RANGE_ERROR;
			}
			++calls; // This fixture's actual issue frontier.
			NativeCallFact fact;
			fact.serial = 17;
			fact.completion_serial = 73;
			fact.queue_index = 1;
			fact.issue = unknown_issue ? IssueFact::IssuedUnknown : IssueFact::KnownAccepted;
			fact.error = unknown_issue ? FAILED : OK;
			fact.native_code = unknown_issue ? -9 : 0;
			fact.completion = CompletionCapability::Fence;
			finish_native_fact(index, fact, 91);
			return fact.error;
		}
	};
	struct DeviceCleanup {
		RenderingDevice &device;
		~DeviceCleanup() {
			device.frames.clear();
			device.driver = nullptr;
		}
	};
	static Member member() {
		Member result;
		result.source.serial = 17;
		result.source.completion_serial = 73;
		result.source.queue_index = 1;
		result.source.issue = RDD::IssueFact::KnownAccepted;
		result.source.completion = RDD::CompletionCapability::Fence;
		return result;
	}

public:
	static void incremental_credits() {
		using namespace GDExtensionDeviceExit;
		QualificationFactCredits credits;
		CHECK(credits.initialize(4096, 528, 2645, 5) == OK);
		CHECK(credits.occupied() == 3178);
		CHECK(credits.reserve_workers(918) == OK);
		CHECK(credits.occupied() == 4096);
		CHECK(credits.reserve_workers(1) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(credits.occupied() == 4096);
		CHECK(credits.admit_native(2, true) == OK);
		CHECK(credits.pending == 2);
		CHECK(credits.occupied() == 4096);
		CHECK(credits.commit_native(1) == OK);
		CHECK(credits.cancel_native(1, true) == OK);
		CHECK(credits.pending == 0);
		CHECK(credits.committed == 1);
		CHECK(credits.release_worker_quote(917) == OK);
		CHECK(credits.occupied() == 3179);
		CHECK(credits.commit_graph() == OK);
		CHECK(credits.committed == 2);
		CHECK(credits.occupied() == 3179);
		CHECK(credits.commit_native(1) == ERR_INVALID_DATA);
		CHECK(credits.committed == 2); // completion never recycles a receipt frontier
		QualificationFactCredits overflow;
		CHECK(overflow.initialize(4096, UINT64_MAX, 0, 0) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(overflow.capacity == 0);
	}
	static void copy_credits_preserve_main_cleanup() {
		using namespace GDExtensionDeviceExit;
		QualificationFactCredits credits;
		REQUIRE(credits.initialize(4096, 528, 1587, 5, 1058) == OK);
		CHECK(credits.occupied() == 3178);
		REQUIRE(credits.reserve_workers(918) == OK);
		REQUIRE(credits.admit_native(1058, false, true) == OK);
		CHECK(credits.copy == 0);
		CHECK(credits.main == 1587);
		CHECK(credits.cleanup == 5);
		CHECK(credits.admit_native(1, false, true) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(credits.main == 1587);
		CHECK(credits.cleanup == 5);
		CHECK(credits.occupied() == 4096);
		REQUIRE(credits.cancel_native(1058, false, true) == OK);
		CHECK(credits.copy == 1058);
		CHECK(credits.pending == 0);
		CHECK(credits.main == 1587);
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		QualificationTicket ticket;
		REQUIRE(ticket.credits.initialize(8, 1, 1, 5, 1) == OK);
		device.frames[0].qualification_tickets.push_back(&ticket);
		REQUIRE(device._qualification_activate_cleanup_credits() == OK);
		CHECK(ticket.credits.cleanup == 5);
		CHECK(ticket.credits.main == 1);
		CHECK(ticket.credits.copy == 1);
		ticket.admission_closed = true;
		REQUIRE(device._qualification_activate_cleanup_credits() == OK);
		CHECK(ticket.credits.cleanup == 0);
		CHECK(ticket.credits.main == 4);
		CHECK(ticket.credits.copy == 3);
		CHECK(ticket.credits.occupied() == 8);
		REQUIRE(device._qualification_activate_cleanup_credits() == OK);
		CHECK(ticket.credits.occupied() == 8);
		device.frames[0].qualification_tickets.clear();
	}
	static void incremental_shared_frame() {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		device.frames[0].qualification_epoch = 81;
		QualificationTicket a, b;
		REQUIRE(a.credits.initialize(12, 1, 5, 1) == OK);
		REQUIRE(b.credits.initialize(12, 1, 1, 1) == OK);
		device.frames[0].qualification_tickets.push_back(&a);
		device.frames[0].qualification_tickets.push_back(&b);
		const uint64_t batch[] = { 17, 18 };
		// Ownership is scoped to the actual Registry and the current thread.
		auto &original_registry = Registry::get_singleton();
		REQUIRE(!Registry::Lock::owns(original_registry));
		{
			Registry *other_registry = memnew(Registry);
			Registry::Lock lock(original_registry);
			CHECK(Registry::Lock::owns(original_registry));
			CHECK(!Registry::Lock::owns(*other_registry));
			{
				Registry::Lock borrowed(original_registry, Registry::Lock::TRY_OR_BORROW);
				CHECK(borrowed.is_locked());
			}
			CHECK(Registry::Lock::owns(original_registry));
			lock.temp_unlock();
			CHECK(!Registry::Lock::owns(original_registry));
			{
				Registry::Lock reacquired(original_registry, Registry::Lock::TRY_OR_BORROW);
				CHECK(reacquired.is_locked());
			}
			CHECK(!Registry::Lock::owns(original_registry));
			lock.temp_relock();
			CHECK(Registry::Lock::owns(original_registry));
			{
				Registry::Lock other(*other_registry);
				CHECK(Registry::Lock::owns(*other_registry));
				CHECK(Registry::Lock::owns(original_registry));
			}
			CHECK(!Registry::Lock::owns(*other_registry));
			CHECK(device._qualification_admit_native_serials(batch, 1, UINT32_MAX) == OK);
			device._qualification_cancel_native_serials(batch, 1, UINT32_MAX);
			memdelete(other_registry);
		}
		CHECK(!Registry::Lock::owns(original_registry));

		CHECK(device._qualification_admit_native_serials(batch, 2, UINT32_MAX) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(a.credits.main == 5);
		CHECK(b.credits.main == 1);
		CHECK(a.admitted_native_serials.is_empty());
		CHECK(b.admitted_native_serials.is_empty());
		CHECK(device._qualification_admit_native_serials(batch, 1, UINT32_MAX) == OK);
		CHECK(a.credits.pending == 1);
		CHECK(b.credits.pending == 1);
		CHECK(device._qualification_admit_native_serials(batch, 1, UINT32_MAX) == OK);
		CHECK(a.credits.pending == 1);
		CHECK(b.credits.pending == 1);
		device._qualification_cancel_native_serials(batch, 1, UINT32_MAX);
		CHECK(a.credits.pending == 0);
		CHECK(b.credits.pending == 0);
		CHECK(a.credits.main == 5);
		CHECK(b.credits.main == 1);
#ifdef THREADS_ENABLED
		auto &registry = Registry::get_singleton();
		struct Contention {
			Registry *registry;
			Semaphore entered{}, release{};
		} contention{ &registry };
		Thread holder;
		holder.start([](void *data) {auto &state=*static_cast<Contention*>(data);Registry::Lock lock(*state.registry);state.entered.post();state.release.wait(); }, &contention);
		contention.entered.wait();
		{
			Registry::Lock refused(registry, Registry::Lock::TRY_OR_BORROW);
			CHECK(!refused.is_locked());
			CHECK(!Registry::Lock::owns(registry));
		}
		CHECK(device._qualification_admit_native_serials(batch, 1, UINT32_MAX) == ERR_BUSY);
		contention.release.post();
		holder.wait_to_finish();
#endif
		CHECK(a.credits.pending == 0);
		CHECK(b.credits.pending == 0);
		// Adopting an existing hidden prefix is also before membership publication.
		device.frames[0].qualification_tickets.clear();
		RDD::QueueCallReport hidden;
		hidden.calls.push_back(member().source);
		hidden.issued = true;
		device.frames[0].qualification_hidden_reports.push_back(hidden);
		QualificationTicket c;
		REQUIRE(c.credits.initialize(8, 1, 6, 1) == OK);
		CHECK(device._qualification_admit_frame_ticket(&c) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(c.frame_refs == 0);
		CHECK(device.frames[0].qualification_tickets.is_empty());
		CHECK(c.admitted_native_serials.is_empty());
		QualificationTicket d;
		REQUIRE(d.credits.initialize(9, 1, 6, 1) == OK);
		CHECK(device._qualification_admit_frame_ticket(&d) == OK);
		CHECK(d.frame_refs == 1);
		CHECK(d.credits.pending == 1);
		CHECK(d.admitted_native_serials.size() == 1);
		REQUIRE(d.credits.commit_graph() == OK);
		GDExtensionQualificationFactV1 graph{};
		graph.command_slot = 0;
		d.facts.push_back(graph);
		d.graphs.push_back({ {}, 0, 0, 0, 81, true, false });
		device._qualification_add_frame_ticket(&d);
		REQUIRE(d.natives.size() == 1);
		CHECK(d.natives[0].source.serial == 17);
		CHECK(d.credits.pending == 0);
		CHECK(d.credits.committed == 2);
		device._qualification_add_frame_ticket(&d);
		CHECK(d.facts.size() == 2);
		CHECK(d.natives.size() == 1);
		device.frames[0].qualification_hidden_reports.clear();
	}
	static void incremental_worker_batch() {
		using namespace GDExtensionDeviceExit;
		struct ExactDriver : Driver {
			uint64_t serial = 17;
			Error qualification_worker_source_quote(CommandBufferID, QueueCallReport &prefix, uint32_t &future) const override {
				prefix = {};
				future = 2;
				return OK;
			}
			Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
				const Error admission = admit_native_source(serial);
				if (admission != OK) {
					QueueCallReport report;
					report.error = admission;
					record_queue_call_report(report);
					return admission;
				}
				uint32_t fact_index = UINT32_MAX;
				if (!claim_native_fact(fact_index)) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				++calls;
				NativeCallFact source;
				source.serial = serial;
				source.completion_serial = serial;
				source.queue_index = 1;
				source.issue = IssueFact::IssuedUnknown;
				source.completion = CompletionCapability::CommandBuffer;
				finish_native_fact(fact_index, source, serial);
				return OK;
			}
			Error native_call_retain(uint64_t source) override {
				CHECK(source == serial);
				++retains;
				return OK;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		device.frames[0].qualification_epoch = 81;
		device.frames[0].transfer_worker_semaphores.push_back(RDD::SemaphoreID(62));
		QualificationTicket a, b;
		struct WorkerFixtureCleanup {
			RenderingDevice &device;
			RenderingDevice::TransferWorker *worker = nullptr;
			~WorkerFixtureCleanup() {
				for (auto &frame : device.frames) {
					frame.qualification_tickets.clear();
					frame.qualification_hidden_reports.clear();
				}
				device.transfer_worker_pool.clear();
				device.transfer_worker_pool_available_list.clear();
				device.transfer_worker_operation_used_by_draw.clear();
				device.transfer_worker_pool_size = device.transfer_worker_pool_max_size = 0;
				if (worker) {
					memdelete(worker);
				}
			}
		} worker_cleanup{ device };
		REQUIRE(a.credits.initialize(32, 4, 20, 1) == OK);
		REQUIRE(b.credits.initialize(32, 4, 20, 1) == OK);
		device.frames[0].qualification_tickets.push_back(&a);
		device.frames[0].qualification_tickets.push_back(&b);
		auto *worker = memnew(RenderingDevice::TransferWorker);
		worker_cleanup.worker = worker;
		worker->index = 0;
		worker->operations_counter = 9;
		worker->recording = true;
		worker->command_buffer = RDD::CommandBufferID(61);
		worker->command_fence = RDD::FenceID(91);
		device.transfer_worker_pool.push_back(worker);
		device.transfer_worker_pool_size = device.transfer_worker_pool_max_size = 1;
		device.transfer_worker_operation_used_by_draw.resize(1);
		device.transfer_worker_operation_used_by_draw[0] = 0;
		device.transfer_worker_pool_available_list.push_back(0);
		RenderingDevice::Buffer buffer;
		buffer.transfer_worker_index = 0;
		buffer.transfer_worker_operation = 9;
		REQUIRE(b.credits.reserve_workers(7) == OK);
		CHECK(device._check_transfer_worker_buffer(&buffer) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(buffer.transfer_worker_index == 0);
		CHECK(device.transfer_worker_operation_used_by_draw[0] == 0);
		CHECK_FALSE(worker->qualification_batch_sealed);
		CHECK(driver.calls == 0);
		CHECK(a.credits.workers == 0);
		CHECK(a.worker_credits.is_empty());
		REQUIRE(b.credits.release_worker_quote(7) == OK);
		CHECK(device._check_transfer_worker_buffer(&buffer) == OK);
		CHECK(buffer.transfer_worker_index == -1);
		CHECK(device.transfer_worker_operation_used_by_draw[0] == 9);
		CHECK(worker->qualification_batch_sealed);
		CHECK(a.credits.workers == 2);
		CHECK(b.credits.workers == 2);
		uint32_t offset = 0;
		CHECK(device._acquire_transfer_worker(1, 1, offset) == nullptr); // no wait on this frame's own sealed batch
		CHECK(driver.calls == 0);
		auto parent = [&](QualificationTicket &ticket, uint64_t epoch) {REQUIRE(ticket.credits.commit_graph()==OK);GDExtensionQualificationFactV1 fact{};fact.command_slot=0;ticket.graphs.push_back({{},ticket.facts.size(),0,0,epoch,true,false});ticket.facts.push_back(fact); };
		parent(a, 81);
		parent(b, 81);
		CHECK(device._submit_transfer_workers({}, true) == OK);
		CHECK(driver.calls == 1);
		CHECK_FALSE(worker->qualification_batch_sealed);
		CHECK(a.credits.workers == 0);
		CHECK(b.credits.workers == 0);
		REQUIRE(a.natives.size() == 1);
		CHECK(a.natives[0].source.serial == 17);
		CHECK(a.facts.size() == 2);
		CHECK(b.facts.size() == 2);
		// Same exact submitted source deduplicates, but a later operation and serial
		// on the same physical worker requires a new independent quote/fact.
		device._qualification_issue_tickets(UINT32_MAX, worker->last_submission, OK);
		CHECK(a.facts.size() == 2);
		worker->operations_processed = 9;
		worker->operations_counter = 10;
		worker->recording = true;
		worker->submitted = false;
		driver.serial = 18;
		device.frames[0].qualification_epoch = 82;
		device.frames[0].qualification_hidden_reports.clear();
		buffer.transfer_worker_index = 0;
		buffer.transfer_worker_operation = 10;
		CHECK(device._check_transfer_worker_buffer(&buffer) == OK);
		CHECK(worker->qualification_batch_sealed);
		parent(a, 82);
		parent(b, 82);
		CHECK(device._submit_transfer_workers({}, true) == OK);
		CHECK(driver.calls == 2);
		CHECK_FALSE(worker->qualification_batch_sealed);
		REQUIRE(a.natives.size() == 2);
		CHECK(a.natives[1].source.serial == 18);
		CHECK(a.facts.size() == 4);
		CHECK(a.credits.committed == 4);
		CHECK(a.credits.workers == 0);
	}
	static void deferred_free_busy_root() {
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		RenderingDevice::Buffer buffer;
		buffer.draw_tracker = RDG::resource_tracker_create();
		buffer.qualification_users = 1;
		const RID rid = device.uniform_buffer_owner.make_rid(buffer);
		device._qualification_defer_free(rid);
		device._qualification_defer_free(rid);
		REQUIRE(device.qualification_deferred_frees.size() == 1);
		ERR_PRINT_OFF;
		device._qualification_retry_deferred_frees();
		device._qualification_retry_deferred_frees();
		ERR_PRINT_ON;
		CHECK(device.uniform_buffer_owner.owns(rid));
		REQUIRE(device.qualification_deferred_frees.size() == 1);
		CHECK(device.qualification_deferred_frees[0] == rid);
		CHECK(device.frames[0].buffers_to_dispose_of.is_empty());
		device.uniform_buffer_owner.get_or_null(rid)->qualification_users = 0;
		device._qualification_retry_deferred_frees();
		CHECK_FALSE(device.uniform_buffer_owner.owns(rid));
		CHECK(device.qualification_deferred_frees.is_empty());
		CHECK(device.frames[0].buffers_to_dispose_of.size() == 1); // queued, not a physical-return claim
	}
	static void copy_source_segment_retry() {
		using namespace GDExtensionDeviceExit;
		struct CopyDriver : Driver {
			uint32_t segment_calls = 0;
			Error end_segment_checked(bool) override {
				++segment_calls;
				if (segment_calls > 1) {
					return OK;
				}
				const uint64_t serial = 17;
				const Error error = admit_native_source(serial, true);
				if (error != OK) {
					return error;
				}
				++calls;
				QueueCallReport report;
				NativeCallFact source;
				source.serial = serial;
				source.completion_serial = serial;
				source.queue_index = 2;
				source.issue = IssueFact::IssuedUnknown;
				source.completion = CompletionCapability::CommandBuffer;
				source.frame_observer_owned = true;
				report.calls.push_back(source);
				report.issued = true;
				report.native_issue_count = 1;
				CHECK(capture_hidden_native_source(report));
				return ERR_BUSY;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		auto &frame = device.frames[0];
		frame.qualification_epoch = 81;
		frame.qualification_graph_closed = true;
		QualificationTicket ticket;
		REQUIRE(ticket.credits.initialize(8, 1, 1, 1, 1) == OK);
		REQUIRE(ticket.credits.commit_graph() == OK);
		GDExtensionQualificationFactV1 graph{};
		graph.command_slot = 0;
		ticket.facts.push_back(graph);
		ticket.graphs.push_back({ {}, 0, 0, 0, 81, true, false });
		frame.qualification_tickets.push_back(&ticket);
		CHECK(device._end_frame(true) == ERR_BUSY);
		CHECK(driver.calls == 1);
		CHECK(driver.segment_calls == 1);
		CHECK(frame.qualification_graph_closed);
		CHECK_FALSE(frame.qualification_segment_closed);
		REQUIRE(frame.qualification_hidden_reports.size() == 1);
		REQUIRE(ticket.natives.size() == 1);
		CHECK(ticket.natives[0].source.serial == 17);
		CHECK(ticket.credits.pending == 0);
		CHECK(ticket.credits.committed == 2);
		CHECK_FALSE(ticket.append_failed);
		CHECK(device._end_frame(true) == OK);
		CHECK(frame.qualification_segment_closed);
		CHECK(driver.segment_calls == 2);
		CHECK(driver.calls == 1);
		CHECK(device._end_frame(true) == OK);
		CHECK(driver.segment_calls == 2);
		CHECK(ticket.natives.size() == 1);
		CHECK(ticket.facts.size() == 2);
		frame.qualification_tickets.clear();
		frame.qualification_hidden_reports.clear();
	}
	static void enrollment_pool_refusal_restores_registry() {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		device.frames[0].qualification_epoch = 81;
		QualificationTicket ticket;
		REQUIRE(ticket.credits.initialize(8, 1, 1, 1, 1) == OK);
#ifdef THREADS_ENABLED
		struct PoolHolder {
			BinaryMutex *pool;
			Semaphore entered{}, release{};
		} state{ &device.transfer_worker_pool_mutex };
		Thread holder;
		holder.start([](void *p) {auto &probe=*static_cast<PoolHolder*>(p);probe.pool->lock();probe.entered.post();probe.release.wait();probe.pool->unlock(); }, &state);
		state.entered.wait();
		auto &registry = Registry::get_singleton();
		{
			Registry::Lock lock(registry);
			{
				RenderingDevice::QualificationSourceAdmissionScope scope(device);
				CHECK_FALSE(RDD::native_source_can_block());
				lock.temp_unlock();
				CHECK(RDD::native_source_can_block());
				lock.temp_relock();
				CHECK_FALSE(RDD::native_source_can_block());
			}
			CHECK(device._qualification_admit_frame_ticket(&ticket) == ERR_BUSY);
			CHECK(Registry::Lock::owns(registry));
			CHECK_FALSE(ticket.recording);
			CHECK(ticket.frame_refs == 0);
			CHECK(ticket.credits.workers == 0);
			CHECK(ticket.admitted_native_serials.is_empty());
			CHECK(device.frames[0].qualification_tickets.is_empty());
		}
		state.release.post();
		holder.wait_to_finish();
#endif
		CHECK(device._qualification_admit_frame_ticket(&ticket) == OK);
		CHECK_FALSE(ticket.recording);
		CHECK(ticket.frame_refs == 1);
		device.frames[0].qualification_tickets.clear();
	}

	static void hidden_root_failure_retirement() {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		auto &frame = device.frames[0];
		frame.qualification_epoch = 81;
		RenderingDevice::Buffer buffer;
		buffer.draw_tracker = RDG::resource_tracker_create();
		const RID rid = device.uniform_buffer_owner.make_rid(buffer);
		QualificationTicket ticket;
		REQUIRE(ticket.credits.initialize(8, 1, 1, 1, 1) == OK);
		ticket.frame_refs = 1;
		frame.qualification_tickets.push_back(&ticket);
		{
			Registry::Lock lock(Registry::get_singleton());
			LocalVector<RID> resources;
			resources.push_back(rid);
			REQUIRE(device._qualification_pin_resources(&ticket, resources) == OK);
		}
		RDD::QueueCallReport report;
		auto source = member().source;
		source.queue_index = 2;
		source.completion_serial = 17;
		source.issue = RDD::IssueFact::IssuedUnknown;
		source.completion = RDD::CompletionCapability::CommandBuffer;
		source.frame_observer_owned = true;
		report.calls.push_back(source);
		report.issued = true;
		report.native_issue_count = 1;
		{
			RenderingDevice::QualificationSourceAdmissionScope scope(device);
			REQUIRE(RDD::admit_native_source(17, true) == OK);
			REQUIRE(RDD::capture_hidden_native_source(report));
		}
		REQUIRE(ticket.facts.size() == 1);
		REQUIRE(ticket.natives.size() == 1);
		CHECK(ticket.facts[0].parent_fact == UINT32_MAX);
		CHECK(ticket.graphs.is_empty());
		CHECK(ticket.facts[0].native_serial == 17);
		CHECK(ticket.credits.committed == 1);
		CHECK(ticket.report.state != 8);
		CHECK_FALSE(ticket.physically_retired.load());
		driver.observed.completed = true;
		driver.observed.completed_serial = 17;
		driver.observed.resolved_issue = RDD::IssueFact::KnownAccepted;
		REQUIRE(device._qualification_poll_tickets(0) == OK);
		CHECK(ticket.facts[0].completion == 2);
		CHECK(ticket.report.state != 8);
		CHECK_FALSE(ticket.physically_retired.load());
		// The original Frame observer returns through its actual hidden-report path.
		REQUIRE(device._stall_for_frame(0, true) == OK);
		CHECK(frame.qualification_hidden_reports.is_empty());
		CHECK(driver.releases == 1);
		ticket.append_failed = true;
		ticket.report.native_error = ERR_CANT_CREATE;
		// Controlled original frame-return callback, with no invented graph/main fact.
		device._qualification_return_frame_tickets(0);
		CHECK(ticket.physically_retired.load());
		CHECK(ticket.report.state != 8);
		CHECK(ticket.facts.size() == 1);
		CHECK(ticket.facts[0].parent_fact == UINT32_MAX);
		CHECK(ticket.resource_pins.is_empty());
		CHECK(device.uniform_buffer_owner.get_or_null(rid)->qualification_users == 0);
		REQUIRE(release_native_source(driver, ticket.natives[0]) == OK);
		CHECK(driver.releases == 2);
		CHECK(ticket.natives[0].owner_released);
		device.free_rid(rid);
		CHECK_FALSE(device.uniform_buffer_owner.owns(rid));
		CHECK(frame.buffers_to_dispose_of.size() == 1);
	}
	static void hidden_root_never_maps_missing_main() {
		using namespace GDExtensionDeviceExit;
		struct GraphDriver : Driver {
			uint32_t copies = 0;
			void command_copy_buffer(CommandBufferID, BufferID from, BufferID to, VectorView<BufferCopyRegion> regions) override {
				CHECK(from == BufferID(5));
				CHECK(to == BufferID(6));
				REQUIRE(regions.size() == 1);
				CHECK(regions[0].size == 4);
				++copies;
			}
		} driver;
		RenderingDevice device;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		device.frames.resize(1);
		auto &frame = device.frames[0];
		frame.qualification_epoch = 81;
		QualificationTicket ticket;
		REQUIRE(ticket.credits.initialize(8, 1, 1, 1, 1) == OK);
		ticket.frame_refs = 1;
		frame.qualification_tickets.push_back(&ticket);
		RDD::QueueCallReport report;
		auto source = member().source;
		source.queue_index = 2;
		source.completion_serial = 17;
		source.issue = RDD::IssueFact::IssuedUnknown;
		source.completion = RDD::CompletionCapability::CommandBuffer;
		report.calls.push_back(source);
		report.issued = true;
		{
			RenderingDevice::QualificationSourceAdmissionScope scope(device);
			REQUIRE(RDD::admit_native_source(17, true) == OK);
			REQUIRE(RDD::capture_hidden_native_source(report));
		}
		auto *from = RDG::resource_tracker_create();
		auto *to = RDG::resource_tracker_create();
		from->buffer_driver_id = RDD::BufferID(5);
		to->buffer_driver_id = RDD::BufferID(6);
		const auto handle = device.draw_graph.add_buffer_copy(RDD::BufferID(5), from, RDD::BufferID(6), to, { 0, 0, 4 });
		REQUIRE(handle.is_valid());
		{
			auto &registry = Registry::get_singleton();
			Registry::Lock lock(registry);
			// Actual production add_graph initializes the unsealed command slot to UINT32_MAX.
			registry.qualification_add_upload_graph(&ticket, &device, handle.graph_instance, handle.graph_frame, handle.kind, handle.index, handle.offset, 5, 0, 4);
		}
		REQUIRE(ticket.graphs.size() == 1);
		CHECK_FALSE(ticket.graphs[0].sealed);
		CHECK(ticket.facts[1].command_slot == UINT32_MAX);
		CHECK(ticket.facts[1].native_serial == 0);
		CHECK(ticket.facts[1].issue == 0);
		driver.observed.completed = true;
		driver.observed.completed_serial = 17;
		driver.observed.resolved_issue = RDD::IssueFact::KnownAccepted;
		REQUIRE(device._qualification_poll_tickets(0) == OK);
		CHECK(ticket.facts[1].native_serial == 0);
		CHECK(ticket.facts[1].issue == 0);
		RDD::CommandBufferID primary(60);
		device.draw_graph.end(true, false, primary, frame.command_buffer_pool);
		device._qualification_seal_tickets();
		CHECK(driver.copies == 1);
		CHECK(ticket.graphs[0].sealed);
		CHECK(ticket.facts[1].command_slot == 0);
		CHECK(ticket.facts[1].native_serial == 0);
		CHECK(ticket.facts[1].issue == 0);
		CHECK(ticket.facts[0].parent_fact == UINT32_MAX); // Never attach a later arbitrary graph.
		// Make every independent success prerequisite true. The missing original
		// main mapping alone must still prevent READY after the hidden source returns.
		ticket.recorded = true;
		ticket.ordering_closed = true;
		ticket.frame_refs = 0;
		ticket.graphs[0].returned = true;
		ticket.origin.observed_backing_serial = 5;
		ticket.origin.readback_size = 4;
		ticket.original_readback_serial = 23;
		ticket.result.instantiate();
		REQUIRE(ticket.result->bytes.resize(4) == OK);
		ticket.result->request_serial = 23;
		ticket.result->source_backing_serial = 5;
		ticket.result->source_size = 4;
		ticket.result->issued_size = 4;
		ticket.result->completed_size = 4;
		ticket.result->delivered = true;
		CHECK(ticket.readback_complete(0));
		driver.observed.completed = true;
		driver.observed.completed_serial = 17;
		driver.observed.resolved_issue = RDD::IssueFact::KnownAccepted;
		REQUIRE(device._qualification_poll_tickets(0) == OK);
		CHECK(ticket.facts[0].completion == 2);
		CHECK(ticket.facts[1].issue == 0);
		CHECK(ticket.facts[1].native_serial == 0);
		CHECK(ticket.report.state != 8);
		CHECK_FALSE(ticket.physically_retired.load());
		REQUIRE(release_native_source(driver, ticket.natives[0]) == OK);
		frame.qualification_tickets.clear();
		frame.qualification_hidden_reports.clear();
		RDG::resource_tracker_free(from);
		RDG::resource_tracker_free(to);
	}

	static void bounds() {
		using namespace GDExtensionDeviceExit;
		uint32_t slots = 99, frames = 99;
		CHECK(record_source_slot_bound(2, 3, 5, 4, slots, frames) == OK);
		CHECK(slots == 33);
		CHECK(frames == 5);
		CHECK(record_source_slot_bound(0, 3, 5, 4, slots, frames) == ERR_UNAVAILABLE);
		CHECK(slots == 0);
		CHECK(frames == 0);
		CHECK(record_source_slot_bound(1, UINT64_MAX, 0, 0, slots, frames) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(record_source_slot_bound(1, 1, UINT32_MAX, UINT32_MAX, slots, frames) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(qualification_source_slot_bound(3, 0, 2, 12, slots) == OK);
		CHECK(slots == 12);
		CHECK(qualification_source_slot_bound(3, 0, 2, 11, slots) == ERR_PARAMETER_RANGE_ERROR);
		CHECK(slots == 0);
		CHECK(qualification_source_slot_bound(3, 0, 2, 13, slots) == OK);
		CHECK(slots == 12);
	}
	static void native_source() {
		using namespace GDExtensionDeviceExit;
		static_assert(!std::is_default_constructible_v<Origin>);
		Driver driver;
		auto original = member();
		const auto qualification = Origin::qualification();
		driver.retain_error = ERR_BUSY;
		CHECK(poll_native_source(driver, original, qualification).state == Observation::State::Pending);
		CHECK_FALSE(original.retained);
		CHECK(driver.polls == 0);
		driver.retain_error = OK;
		driver.observed = { true, false, 72, 0 };
		CHECK(poll_native_source(driver, original, qualification).state == Observation::State::Pending);
		CHECK(original.retained);
		CHECK(driver.retains == 2);
		driver.observed.completed_serial = 73;
		CHECK(poll_native_source(driver, original, qualification).state == Observation::State::Complete);
		CHECK(original.source.issue == RDD::IssueFact::KnownAccepted);
		CHECK(driver.retains == 2);
		auto uncertain = member();
		uncertain.source.issue = RDD::IssueFact::IssuedUnknown;
		uncertain.source.error = FAILED;
		uncertain.source.native_code = -9;
		CHECK(poll_native_source(driver, uncertain, qualification).state == Observation::State::Unknown);
		CHECK(uncertain.source.issue == RDD::IssueFact::IssuedUnknown);
		CHECK(uncertain.source.native_code == -9);
		const auto production = Origin::production();
		CHECK(poll_native_source(driver, uncertain, production).state == Observation::State::Unknown);
		// A driver-owned same-command callback may resolve a void commit. The
		// original raw issue remains unchanged, and an actual issue error cannot
		// be laundered by this completion field.
		driver.observed.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(poll_native_source(driver, uncertain, qualification).state == Observation::State::Unknown);
		uncertain.source.error = OK;
		CHECK(poll_native_source(driver, uncertain, qualification).state == Observation::State::Complete);
		CHECK(uncertain.source.issue == RDD::IssueFact::IssuedUnknown);
		driver.observed = { false, true, 73, -21 };
		auto failed = poll_native_source(driver, original, qualification);
		CHECK(failed.state == Observation::State::NativeError);
		CHECK(failed.error == FAILED);
		CHECK(failed.native_code == -21);
		CHECK(failed.completion_error == FAILED);
		const uint32_t before = driver.polls;
		uncertain.source.error = ERR_UNAVAILABLE;
		uncertain.source.native_code = -9;
		auto combined = poll_native_source(driver, uncertain, production);
		CHECK(driver.polls == before + 1);
		CHECK(combined.error == ERR_UNAVAILABLE);
		CHECK(combined.native_code == -9);
		CHECK(combined.completion_error == FAILED);
		CHECK(combined.completion_native_code == -21);
		CHECK(combined.state == Observation::State::Unknown);
		driver.observed.native_error = false;
		combined = poll_native_source(driver, uncertain, production);
		CHECK(driver.polls == before + 2);
		CHECK(combined.error == ERR_UNAVAILABLE);
		CHECK(combined.completion_error == OK);
		auto unretained = member();
		driver.retain_error = ERR_BUSY;
		CHECK(poll_native_source(driver, unretained, production).completion_error == OK);
		driver.retain_error = ERR_INVALID_DATA;
		CHECK(poll_native_source(driver, unretained, production).completion_error == OK);
		CHECK(driver.polls == before + 2);
		driver.retain_error = OK;
		driver.release_error = ERR_BUSY;
		CHECK(release_native_source(driver, original) == ERR_BUSY);
		CHECK_FALSE(original.owner_released);
		driver.release_error = OK;
		CHECK(release_native_source(driver, original) == OK);
		CHECK(original.owner_released);
		CHECK(release_native_source(driver, original) == OK);
		CHECK(driver.releases == 2);
		const uint32_t released_polls = driver.polls;
		CHECK(poll_native_source(driver, original, production).completion_error == OK);
		CHECK(driver.polls == released_polls);
		CHECK(driver.waits == 0);
	}
	static void bounded_capture() {
		Driver driver;
		RDD::NativeCallFact cells[2];
		cells[1].serial = 123;
		RDD::NativeFactWriteRange range;
		range.data = cells;
		range.capacity = 0;
		CHECK(driver.command_queue_execute_recorded(RDD::CommandQueueID(1), {}, RDD::CommandBufferID(2), {}, RDD::FenceID(3), range) == ERR_UNAVAILABLE);
		CHECK(driver.calls == 0);
		CHECK(range.written == 0);
		range.capacity = 1;
		CHECK(driver.command_queue_execute_recorded(RDD::CommandQueueID(1), {}, RDD::CommandBufferID(2), {}, RDD::FenceID(3), range) == OK);
		CHECK(driver.calls == 1);
		CHECK(range.written == 1);
		CHECK(range.issued);
		CHECK_FALSE(range.coverage_lost);
		CHECK(cells[0].serial == 17);
		CHECK(cells[0].completion_serial == 73);
		CHECK(cells[1].serial == 123);
		CHECK(range.submission_serial == 91);
		CHECK(driver.get_last_queue_call_report().calls.is_empty());
		RDD::QueueCallReport report;
		driver.unknown_issue = true;
		CHECK(driver.command_queue_execute_and_present_observed(RDD::CommandQueueID(1), {}, RDD::CommandBufferID(2), {}, RDD::FenceID(3), {}, report) == FAILED);
		REQUIRE(report.calls.size() == 1);
		CHECK(report.calls[0].issue == RDD::IssueFact::IssuedUnknown);
		CHECK(report.calls[0].native_code == -9);
		CHECK(report.issued);
		CHECK(report.submission_serial == 91);
		CHECK(driver.get_last_queue_call_report().calls[0].serial == 17);
	}
	static void request_identity() {
		using namespace GDExtensionDeviceExit;
		const OriginalOperationIdentity owner{ 11, 12, 13, 14, 0, 4 };
		const OriginalOperationIdentity changed{ 11, 12, 13, 14, 0, 5 };
		CHECK(valid_original_operation_identity(owner));
		CHECK(same_original_operation_identity(owner, owner));
		CHECK_FALSE(same_original_operation_identity(owner, changed));
		CheckedRecordAppend observation;
		CHECK(bind_checked_record_append(observation, owner) == OK);
		CHECK(observation.operation_bound);
		CHECK(same_original_operation_identity(observation.operation, owner));
		CHECK(bind_checked_record_append(observation, changed) == ERR_UNAUTHORIZED);
		const OriginalOperationIdentity invalid{ 11, 12, 13, 14, 4, 4 };
		CHECK_FALSE(valid_original_operation_identity(invalid));
		CHECK(bind_checked_record_append(observation, invalid) == ERR_INVALID_DATA);
		const ReadbackSourceState complete{ 101, 11, 4, 8, 8, 8, 8, OK, true };
		CHECK(same_request_readback_complete(complete, 101, 11, 4, 8));
		for (uint32_t mode = 0; mode < 10; ++mode) {
			auto changed_readback = complete;
			switch (mode) {
				case 0:
					++changed_readback.request_serial;
					break;
				case 1:
					++changed_readback.backing_serial;
					break;
				case 2:
					++changed_readback.offset;
					break;
				case 3:
					++changed_readback.size;
					break;
				case 4:
					--changed_readback.issued;
					break;
				case 5:
					--changed_readback.completed;
					break;
				case 6:
					--changed_readback.owned_bytes;
					break;
				case 7:
					changed_readback.error = ERR_CANT_ACQUIRE_RESOURCE;
					break;
				case 8:
					changed_readback.delivered = false;
					break;
				case 9:
					changed_readback.request_serial = 0;
					break;
			}
			CAPTURE(mode);
			CHECK_FALSE(same_request_readback_complete(changed_readback, 101, 11, 4, 8));
		}
		CHECK_FALSE(same_request_readback_complete(complete, 101, 11, UINT64_MAX, 8));
	}
	static void checked_append() {
		using State = GDExtensionDeviceExit::CheckedRecordAppend::State;
		const GDExtensionDeviceExit::OriginalOperationIdentity identity{ 11, 12, 13, 14, 0, 4 };
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDevice::Buffer from, to;
		from.driver_id = RDD::BufferID(5);
		from.size = 32;
		from.backing_serial = 5;
		to.driver_id = RDD::BufferID(6);
		to.size = 32;
		to.backing_serial = 6;
		from.draw_tracker = RDG::resource_tracker_create();
		from.draw_tracker->buffer_driver_id = from.driver_id;
		to.draw_tracker = RDG::resource_tracker_create();
		to.draw_tracker->buffer_driver_id = to.driver_id;
		RID src = device.storage_buffer_owner.make_rid(from), dst = device.storage_buffer_owner.make_rid(to);
		uint32_t before = device.draw_graph.qualification_command_count();
		const GDExtensionDeviceExit::OriginalOperationIdentity missing{};
		auto denied = device._record_source_buffer_copy(missing, src, dst, 0, 0, 16);
		CHECK(denied.state == State::Rejected);
		CHECK(denied.error == ERR_INVALID_DATA);
		CHECK_FALSE(denied.operation_bound);
		CHECK(device.draw_graph.qualification_command_count() == before);
		ERR_PRINT_OFF;
		auto bad = device._record_source_buffer_copy(identity, src, dst, 31, 0, 2);
		CHECK(bad.state == State::Rejected);
		CHECK(bad.error == ERR_INVALID_PARAMETER);
		CHECK(device.draw_graph.qualification_command_count() == before);
		CHECK(device.buffer_copy(src, dst, 31, 0, 2) == ERR_INVALID_PARAMETER);
		ERR_PRINT_ON;
		auto appended = device._record_source_buffer_copy(identity, src, dst, 0, 0, 16);
		CHECK(appended.state == State::Appended);
		CHECK(appended.error == OK);
		CHECK(appended.operation_bound);
		CHECK(GDExtensionDeviceExit::same_original_operation_identity(appended.operation, identity));
		CHECK(device.draw_graph.contains_record(appended.handle));
		CHECK(device.draw_graph.qualification_command_count() == before + 1);
		device.storage_buffer_owner.get_or_null(dst)->draw_tracker = nullptr;
		before = device.draw_graph.qualification_command_count();
		auto mutable_copy = device._record_source_buffer_copy(identity, src, dst, 0, 0, 16);
		CHECK(mutable_copy.error == OK);
		CHECK(mutable_copy.state == State::Appended);
		CHECK(mutable_copy.became_mutable);
		CHECK(device.draw_graph.contains_record(mutable_copy.handle));
		CHECK(mutable_copy.transfer_count == 0);
		auto *mutable_tracker = device.storage_buffer_owner.get_or_null(dst)->draw_tracker;
		before = device.draw_graph.qualification_command_count();
		device.compute_list.active = true;
		CHECK(device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 0, 1, 1).error == ERR_INVALID_PARAMETER);
		CHECK(device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 1, 1, 1).error == ERR_INVALID_DATA);
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(7);
		RID shader_rid = device.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(8);
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.shader = shader_rid;
		pipeline.push_constant_size = 16;
		RID pipeline_rid = device.compute_pipeline_owner.make_rid(pipeline);
		device.compute_list.state.pipeline = pipeline_rid;
		device.compute_list.state.pipeline_shader = shader_rid;
		device.compute_list.state.pipeline_shader_driver_id = shader.driver_id;
#ifdef DEBUG_ENABLED
		device.compute_list.validation.pipeline_active = true;
		device.compute_list.validation.pipeline_push_constant_size = 16;
#endif
		CHECK(device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 1, 1, 1).error == ERR_UNAVAILABLE);
		CHECK(device.draw_graph.qualification_command_count() == before);
		device.compute_pipeline_owner.get_or_null(pipeline_rid)->push_constant_size = 0;
		device.compute_list.state.pipeline_shader_driver_id = shader.driver_id;
#ifdef DEBUG_ENABLED
		device.compute_list.validation.pipeline_active = true;
		device.compute_list.validation.pipeline_push_constant_size = 0;
#endif
		device.draw_graph.add_compute_list_begin();
		auto dispatched = device._record_source_dispatch(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, 1, 1, 1);
		CHECK(dispatched.state == State::Appended);
		CHECK(dispatched.error == OK);
		CHECK(GDExtensionDeviceExit::same_original_operation_identity(dispatched.operation, identity));
		CHECK(device.draw_graph.contains_record(dispatched.handle));
		device.draw_graph.add_compute_list_end();
		device.compute_list.active = false;
		device.compute_pipeline_owner.free(pipeline_rid);
		device.shader_owner.free(shader_rid);
		RDG::resource_tracker_free(from.draw_tracker);
		RDG::resource_tracker_free(to.draw_tracker);
		RDG::resource_tracker_free(mutable_tracker);
		device.storage_buffer_owner.free(src);
		device.storage_buffer_owner.free(dst);
		device.driver = nullptr;
	}
	static void normal_push_dispatch() {
		using namespace GDExtensionDeviceExit;
		using State = CheckedRecordAppend::State;
		const OriginalOperationIdentity identity{ 41, 42, 43, 44, 0, 4 };
		const uint32_t push[4] = { 0x12345678, 0xabcdef01, 0x87654321, 0x10fedcba };
		RenderingDevice device;
		Driver driver;
		driver.honors_barriers = false;
		driver.expected_uniform_set = 9;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		RenderingDevice::Shader shader;
		shader.driver_id = RDD::ShaderID(7);
		RID shader_rid = device.shader_owner.make_rid(shader);
		RenderingDevice::ComputePipeline pipeline;
		pipeline.driver_id = RDD::PipelineID(8);
		pipeline.shader_driver_id = shader.driver_id;
		pipeline.shader = shader_rid;
		pipeline.push_constant_size = sizeof(push);
		pipeline.set_formats.push_back(123);
		RID pipeline_rid = device.compute_pipeline_owner.make_rid(pipeline);
		RenderingDevice::UniformSet set;
		set.driver_id = RDD::UniformSetID(9);
		set.shader_id = shader_rid;
		set.shader_set = 0;
		set.format = 123;
		RID set_rid = device.uniform_set_owner.make_rid(set);
		const auto list = device.compute_list_begin();
		device.compute_list_bind_compute_pipeline(list, pipeline_rid);
		device.compute_list_bind_uniform_set(list, set_rid, 0);
		auto rejected = [&](const void *data, uint32_t size, Error error) {
			const auto result = device._record_source_dispatch(identity, list, 1, 2, 3, data, size);
			CHECK(result.state == State::Rejected);
			CHECK(result.error == error);
			CHECK(result.operation_bound);
			CHECK_FALSE(result.handle.is_valid());
			CHECK(device.compute_list.state.push_constant_size == 0);
			CHECK(device.compute_list.state.dispatch_count == 0);
			CHECK_FALSE(device.compute_list.state.sets[0].bound);
		};
		rejected(nullptr, sizeof(push), ERR_UNAVAILABLE);
		rejected(push, 12, ERR_UNAVAILABLE);
		rejected(push, 132, ERR_UNAVAILABLE);
		QualificationPhysicalReturn physical;
		auto *owned_pipeline = device.compute_pipeline_owner.get_or_null(pipeline_rid);
		auto *owned_shader = device.shader_owner.get_or_null(shader_rid);
		auto *owned_set = device.uniform_set_owner.get_or_null(set_rid);
		owned_pipeline->qualification_return = &physical;
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_pipeline->qualification_return = nullptr;
		owned_shader->qualification_return = &physical;
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_shader->qualification_return = nullptr;
		owned_set->qualification_return = &physical;
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_set->qualification_return = nullptr;
		owned_set->qualification_dependencies.push_back(&physical);
		rejected(push, sizeof(push), ERR_UNAUTHORIZED);
		owned_set->qualification_dependencies.clear();
		owned_set->format = 124;
		rejected(push, sizeof(push), ERR_UNAVAILABLE);
		owned_set->format = 123;
#ifdef DEBUG_ENABLED
		device.compute_list.validation.pipeline_active = false;
		rejected(push, sizeof(push), ERR_INVALID_DATA);
		device.compute_list.validation.pipeline_active = true;
		device.compute_list.validation.pipeline_push_constant_size = 12;
		rejected(push, sizeof(push), ERR_INVALID_DATA);
		device.compute_list.validation.pipeline_push_constant_size = sizeof(push);
#endif
		CHECK_FALSE(device.qualification_recording);
		const auto ordinary = device._record_source_dispatch(identity, list, 1, 2, 3, push, sizeof(push));
		CHECK(ordinary.error == OK);
		CHECK(ordinary.state == State::Appended);
		CHECK(same_original_operation_identity(ordinary.operation, identity));
		CHECK(device.draw_graph.contains_record(ordinary.handle));
		CHECK(device.compute_list.state.push_constant_size == sizeof(push));
		CHECK(memcmp(device.compute_list.state.push_constant_data, push, sizeof(push)) == 0);
		CHECK(device.compute_list.state.sets[0].bound);
		CHECK_FALSE(device.qualification_recording);
		// Existing qualification context remains distinct; this is a fake-owner
		// control, not a production admission grant or a new flag setter API.
		device.qualification_recording = true;
		owned_pipeline->qualification_return = &physical;
		owned_shader->qualification_return = &physical;
		owned_set->qualification_return = &physical;
		device.compute_list_bind_uniform_set(list, set_rid, 0);
		const auto qualified = device._record_source_dispatch(identity, list, 1, 2, 3, push, sizeof(push));
		device.qualification_recording = false;
		CHECK(qualified.error == OK);
		CHECK(qualified.state == State::Appended);
		CHECK(device.draw_graph.contains_record(qualified.handle));
		CHECK(ordinary.handle.graph_instance == qualified.handle.graph_instance);
		CHECK(ordinary.handle.graph_frame == qualified.handle.graph_frame);
		CHECK(ordinary.handle.index == qualified.handle.index);
		CHECK(ordinary.handle.offset < qualified.handle.offset);
		device.compute_list_end();
		CHECK(device.draw_graph.contains_record(ordinary.handle));
		CHECK(device.draw_graph.contains_record(qualified.handle));
		RDG::CommandBufferPool pool;
		RDD::CommandBufferID primary(60);
		device.draw_graph.end(true, false, primary, pool);
		CHECK(driver.push_calls == 2);
		CHECK(driver.prepare_calls == 2);
		CHECK(driver.set_calls == 2);
		CHECK(driver.dispatch_calls == 2);
		CHECK(memcmp(driver.pushed_words, push, sizeof(push)) == 0);
		device.uniform_set_owner.free(set_rid);
		device.compute_pipeline_owner.free(pipeline_rid);
		device.shader_owner.free(shader_rid);
		device.driver = nullptr;
	}

	static void checked_indirect_draw() {
		using namespace GDExtensionDeviceExit;
		using State = CheckedRecordAppend::State;
		const OriginalOperationIdentity identity{ 31, 32, 33, 34, 4, 8 };
		RenderingDevice device;
		struct IndirectDrawDriver : Driver {
			RenderPassID render_pass_create(VectorView<Attachment> p_attachments, VectorView<Subpass> p_subpasses, VectorView<SubpassDependency> p_dependencies, uint32_t p_view_count, AttachmentReference) override {
				CHECK(p_attachments.size() == 0);
				CHECK(p_subpasses.size() == 1);
				CHECK(p_dependencies.size() == 0);
				CHECK(p_view_count == 1);
				return RenderPassID(71);
			}
			void render_pass_free(RenderPassID p_render_pass) override { CHECK(p_render_pass == RenderPassID(71)); }
		} driver;
		device.driver = &driver;
		device.render_thread_id = Thread::get_caller_id();
		device.is_main_instance = false; // Same owner-thread semantics as copy/dispatch.
		device.draw_graph.initialize(&driver, [](RenderingDeviceDriver *, VectorView<RenderingDeviceDriver::AttachmentLoadOp>, VectorView<RenderingDeviceDriver::AttachmentStoreOp>, void *) { return RenderingDeviceDriver::RenderPassID(); }, 1, RDD::CommandQueueFamilyID(1), 0);
		device.draw_graph.begin();
		// Exercise the public begin path; every accepted call below uses its returned ID.
		const RID framebuffer = device.framebuffer_create_empty(Size2i(16, 16));
		REQUIRE(framebuffer.is_valid());
		const RenderingDevice::DrawListID list = device.draw_list_begin(framebuffer);
		REQUIRE(list == (int64_t(RenderingDevice::ID_TYPE_DRAW_LIST) << RenderingDevice::ID_BASE_SHIFT));
		CHECK(list != RenderingDevice::ID_TYPE_DRAW_LIST);
		REQUIRE(device.draw_list.active);
#ifdef DEBUG_ENABLED
		device.draw_list.validation.pipeline_active = true;
		device.draw_list.validation.pipeline_vertex_format = RenderingDevice::INVALID_ID;
#endif
		RenderingDevice::Buffer buffer;
		buffer.driver_id = RDD::BufferID(41);
		buffer.size = 32;
		buffer.usage = RDD::BUFFER_USAGE_INDIRECT_BIT;
		buffer.draw_tracker = RDG::resource_tracker_create();
		buffer.draw_tracker->buffer_driver_id = buffer.driver_id;
		buffer.transfer_worker_index = 0;
		buffer.transfer_worker_operation = 9;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		auto *worker = memnew(RenderingDevice::TransferWorker);
		worker->index = 0;
		device.transfer_worker_pool.push_back(worker);
		device.transfer_worker_pool_size = 1;
		device.transfer_worker_operation_used_by_draw.resize(1);
		device.transfer_worker_operation_used_by_draw[0] = 0;
		const auto missing = device._record_source_draw_indirect({}, list, false, rid, 0, 1, 20);
		CHECK(missing.observed.error == ERR_INVALID_DATA);
		CHECK(missing.observed.state == State::Rejected);
		CHECK_FALSE(missing.observed.operation_bound);
		CHECK(missing.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK(missing.native.prefix_count == 0);
		CHECK_FALSE(missing.native.draw_handle.is_valid());
		device.render_thread_id ^= 1;
		const auto wrong_thread = device._record_source_draw_indirect(identity, list, false, rid, 0, 1, 20);
		device.render_thread_id = Thread::get_caller_id();
		CHECK(wrong_thread.observed.error == ERR_UNAUTHORIZED);
		CHECK(wrong_thread.observed.state == State::Rejected);
		CHECK(wrong_thread.observed.operation_bound);
		CHECK(same_original_operation_identity(wrong_thread.observed.operation, identity));
		CHECK(wrong_thread.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(wrong_thread.native.draw_handle.is_valid());
		const uint32_t command_count = device.draw_graph.qualification_command_count();
		const auto bare_list = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_DRAW_LIST, false, rid, 0, 1, 20);
		CHECK(bare_list.observed.error == ERR_INVALID_PARAMETER);
		CHECK(bare_list.native.error == ERR_INVALID_PARAMETER);
		CHECK(bare_list.observed.state == State::Rejected);
		CHECK(bare_list.observed.operation_bound);
		CHECK(same_original_operation_identity(bare_list.observed.operation, identity));
		CHECK(bare_list.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK(bare_list.native.prefix_count == 0);
		CHECK_FALSE(bare_list.native.capture_complete);
		CHECK_FALSE(bare_list.observed.handle.is_valid());
		CHECK_FALSE(bare_list.native.draw_handle.is_valid());
		CHECK(device.draw_graph.qualification_command_count() == command_count);
		CHECK(device.draw_list.state.draw_count == 0);
		CHECK(device.storage_buffer_owner.get_or_null(rid)->transfer_worker_index == 0);
		CHECK(device.transfer_worker_operation_used_by_draw[0] == 0);
		const auto wrong_list = device._record_source_draw_indirect(identity, RenderingDevice::ID_TYPE_COMPUTE_LIST, false, rid, 0, 1, 20);
		CHECK(wrong_list.observed.error == ERR_INVALID_PARAMETER);
		CHECK(wrong_list.observed.state == State::Rejected);
		CHECK(wrong_list.observed.operation_bound);
		CHECK(wrong_list.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(wrong_list.native.draw_handle.is_valid());
		CHECK(device.draw_list.state.draw_count == 0);
		ERR_PRINT_OFF;
		const auto bad_span = device._record_source_draw_indirect(identity, list, false, rid, 0, 2, 20);
		CHECK(bad_span.observed.error == ERR_INVALID_PARAMETER);
		CHECK(bad_span.native.error == bad_span.observed.error);
		CHECK(bad_span.observed.state == State::Rejected);
		CHECK(bad_span.native.effect == RenderingDevice::NativeAppendEffect::NoNewEffect);
		CHECK_FALSE(bad_span.observed.handle.is_valid());
		CHECK_FALSE(bad_span.native.capture_complete);
		CHECK(bad_span.native.prefix_count == 0);
		CHECK(device.draw_list.state.draw_count == 0);
		CHECK(device.storage_buffer_owner.get_or_null(rid)->transfer_worker_index == 0);
		device.frame_wait_error = ERR_CANT_ACQUIRE_RESOURCE;
		const auto suspended = device._record_source_draw_indirect(identity, list, false, rid, 0, 1, 20);
		device.frame_wait_error = OK;
		ERR_PRINT_ON;
		CHECK(suspended.observed.error == ERR_CANT_ACQUIRE_RESOURCE);
		CHECK(suspended.native.error == ERR_CANT_ACQUIRE_RESOURCE);
		CHECK(suspended.observed.state == State::Rejected);
		const auto drawn = device._record_source_draw_indirect(identity, list, false, rid, 0, 1, 20);
		CHECK(drawn.observed.error == OK);
		CHECK(drawn.observed.state == State::Appended);
		CHECK(drawn.observed.operation_bound);
		CHECK(same_original_operation_identity(drawn.observed.operation, identity));
		CHECK(device.draw_graph.contains_record(drawn.observed.handle));
		CHECK(drawn.observed.handle.graph_instance == drawn.native.draw_handle.graph_instance);
		CHECK(drawn.observed.handle.graph_frame == drawn.native.draw_handle.graph_frame);
		CHECK(drawn.observed.handle.index == drawn.native.draw_handle.index);
		CHECK(drawn.observed.handle.offset == drawn.native.draw_handle.offset);
		CHECK(drawn.native.effect == RenderingDevice::NativeAppendEffect::Recorded);
		CHECK(drawn.native.capture_complete);
		CHECK(drawn.native.has_transfer);
		CHECK(drawn.native.transfer.worker == 0);
		CHECK(drawn.native.transfer.operation == 9);
		CHECK(device.transfer_worker_operation_used_by_draw[0] == 9);
		RenderingDevice::UniformSet set;
		set.format = 1;
		set.driver_id = RDD::UniformSetID(51);
		RID set_rid = device.uniform_set_owner.make_rid(set);
		device.draw_list.state.set_count = 1;
		auto &binding = device.draw_list.state.sets[0];
		binding.uniform_set = set_rid;
		binding.uniform_set_driver_id = set.driver_id;
		binding.pipeline_expected_format = 1;
		binding.uniform_set_format = 1;
		binding.bound = false;
		ERR_PRINT_OFF;
		const auto prefix = device._record_source_draw_indirect(identity, list, true, rid, 0, 1, 20);
		ERR_PRINT_ON;
		CHECK(prefix.observed.error == ERR_INVALID_PARAMETER);
		CHECK(prefix.native.error == prefix.observed.error);
		CHECK(prefix.observed.state == State::Unknown);
		CHECK(same_original_operation_identity(prefix.observed.operation, identity));
		CHECK(prefix.native.effect == RenderingDevice::NativeAppendEffect::NativePrefixOnly);
		CHECK(prefix.native.capture_complete);
		CHECK(prefix.native.prefix_count == 1);
		CHECK(device.draw_graph.contains_record(prefix.native.prefix_handles[0]));
		CHECK_FALSE(prefix.observed.handle.is_valid());
		CHECK(device.draw_list.state.draw_count == 1);
		device.draw_list.validation.index_array_count = 1;
		binding.bound = false;
		const auto indexed = device._record_source_draw_indirect(identity, list, true, rid, 0, 1, 20);
		CHECK(indexed.observed.error == OK);
		CHECK(indexed.observed.state == State::Appended);
		CHECK(same_original_operation_identity(indexed.observed.operation, identity));
		CHECK(device.draw_graph.contains_record(indexed.observed.handle));
		CHECK(indexed.native.prefix_count == 1);
		CHECK(device.draw_graph.contains_record(indexed.native.prefix_handles[0]));
		const auto zero = device._record_source_draw_indirect(identity, list, false, rid, 0, 0, 0);
		CHECK(zero.observed.state == State::Appended);
		CHECK(zero.observed.error == OK);
		CHECK(device.draw_graph.contains_record(zero.native.draw_handle));
		CHECK(device.draw_list.state.draw_count == 3);
		device.draw_graph.add_draw_list_end();
		device.draw_list.active = false;
		RDG::framebuffer_cache_free(&driver, device.framebuffer_owner.get_or_null(framebuffer)->framebuffer_cache);
		device.framebuffer_owner.free(framebuffer);
		for (const KeyValue<RenderingDevice::FramebufferFormatID, RenderingDevice::FramebufferFormat> &format : device.framebuffer_formats) {
			driver.render_pass_free(format.value.render_pass);
		}
		device.framebuffer_formats.clear();
		RDG::resource_tracker_free(buffer.draw_tracker);
		device.storage_buffer_owner.free(rid);
		device.uniform_set_owner.free(set_rid);
		device.transfer_worker_pool.clear();
		device.transfer_worker_pool_size = 0;
		memdelete(worker);
		device.driver = nullptr;
	}

	static void original_readback_identity() {
		GDExtensionDeviceExit::QualificationTicket ticket;
		ticket.original_readback_serial = 101;
		ticket.origin.observed_backing_serial = 11;
		ticket.origin.readback_offset = 4;
		ticket.origin.readback_size = 8;
		ticket.result.instantiate();
		ticket.result->request_serial = 101;
		ticket.result->source_backing_serial = 11;
		ticket.result->source_offset = 4;
		ticket.result->source_size = 8;
		ticket.result->issued_size = ticket.result->completed_size = 8;
		ticket.result->bytes.resize(8);
		ticket.result->delivered = true;
		CHECK(ticket.readback_complete(0));
		ticket.result->request_serial = 102;
		CHECK_FALSE(ticket.readback_complete(0));
		ticket.result->request_serial = 101;
		ticket.result->source_backing_serial = 12;
		CHECK_FALSE(ticket.readback_complete(0));
		ticket.result->source_backing_serial = 11;
		ticket.result->error = ERR_CANT_ACQUIRE_RESOURCE;
		CHECK_FALSE(ticket.readback_complete(0));
	}
	static void original_ticket_poll() {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		Driver driver;
		device.driver = &driver;
		DeviceCleanup cleanup{ device };
		device.frames.resize(1);
		device.frames[0].qualification_epoch = 1;
		QualificationTicket ticket;
		ticket.report.revision = 1;
		GDExtensionQualificationFactV1 graph{};
		graph.command_slot = 0;
		ticket.facts.push_back(graph);
		ticket.graphs.push_back({ {}, 0, 0, 0, 1, true, false });
		device.frames[0].qualification_tickets.push_back(&ticket);
		auto source = member();
		source.source.issue = RDD::IssueFact::IssuedUnknown;
		source.source.error = FAILED;
		source.source.native_code = 88;
		RDD::QueueCallReport report;
		report.calls.push_back(source.source);
		report.error = FAILED;
		report.native_code = 88;
		report.issued = true;
		device._qualification_issue_tickets(0, report, FAILED);
		REQUIRE(ticket.natives.size() == 1);
		const uint32_t native_fact = ticket.natives[0].fact;
		CHECK(ticket.report.native_error == FAILED);
		CHECK(ticket.report.native_code == 88);
		driver.observed = { true, false, 73, 0 };
		driver.retain_error = ERR_BUSY;
		CHECK(device._qualification_poll_tickets(0) == ERR_BUSY);
		CHECK(ticket.facts[native_fact].native_code == 88);
		CHECK(ticket.report.native_code == 88);
		CHECK_FALSE(ticket.natives[0].retained);
		driver.retain_error = ERR_DOES_NOT_EXIST;
		CHECK(device._qualification_poll_tickets(0) == FAILED);
		CHECK(ticket.facts[native_fact].native_code == 88);
		CHECK(ticket.facts[native_fact].error == FAILED);
		CHECK(ticket.report.native_code == 88);
		CHECK(ticket.mapping_failed);
		CHECK_FALSE(ticket.natives[0].retained);
		CHECK(ticket.natives[0].retention_error == ERR_DOES_NOT_EXIST);
		driver.retain_error = OK;
		CHECK(device._qualification_poll_tickets(0) == FAILED);
		CHECK(ticket.facts[native_fact].issue == uint32_t(RDD::IssueFact::IssuedUnknown));
		CHECK(ticket.facts[native_fact].completion == 4);
		CHECK(ticket.report.state == 6);
		CHECK(ticket.facts[native_fact].native_code == 88);
		CHECK(ticket.facts[native_fact].error == FAILED);
		CHECK(ticket.report.native_code == 88);
		CHECK(ticket.natives[0].source.native_code == 88);
		CHECK(ticket.natives[0].completion_native_code == 0);
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(driver.waits == 0);
		QualificationTicket resolved;
		resolved.report.revision = 1;
		auto void_commit = member();
		void_commit.source.issue = RDD::IssueFact::IssuedUnknown;
		resolved.natives.push_back(void_commit);
		resolved.facts.push_back({});
		device.frames[0].qualification_tickets[0] = &resolved;
		driver.observed.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(device._qualification_poll_tickets(0) == OK);
		CHECK(resolved.facts[0].issue == uint32_t(RDD::IssueFact::KnownAccepted));
		CHECK(resolved.facts[0].completion == 2);
		CHECK(resolved.natives[0].source.issue == RDD::IssueFact::IssuedUnknown);
		CHECK(resolved.report.state == 4);
		device.frames.clear();
		device.driver = nullptr;
	}
};
TEST_CASE("[RecordSource] bounded admission arithmetic retains qualification policy") {
	TestRecordSourceAccess::bounds();
}
TEST_CASE("[RecordSource] exact native sources distinguish authoritative issue resolution") {
	TestRecordSourceAccess::native_source();
}
TEST_CASE("[RecordSource] original observed queue uses fixed callsite capture") {
	TestRecordSourceAccess::bounded_capture();
}
TEST_CASE("[RecordSource] readback matches the original request backing and range") {
	TestRecordSourceAccess::request_identity();
}
TEST_CASE("[RecordSource] checked original RD copy preserves Error and actual graph frontier") {
	TestRecordSourceAccess::checked_append();
}
TEST_CASE("[RecordSource] original qualification reducer consumes shared native observations") {
	TestRecordSourceAccess::original_ticket_poll();
}

TEST_CASE("[RecordSource] original ticket readback rejects replaced request and backing") {
	TestRecordSourceAccess::original_readback_identity();
}

TEST_CASE("[RecordSource] original indirect draw retains exact identity native prefix and transfer") {
	TestRecordSourceAccess::checked_indirect_draw();
}

TEST_CASE("[RecordSource] ordinary push and backend descriptor preparation preserve original dispatch") {
	TestRecordSourceAccess::normal_push_dispatch();
}

TEST_CASE("[RecordSource] incremental credits preserve the exact 4096 receipt frontier") {
	TestRecordSourceAccess::incremental_credits();
}
TEST_CASE("[RecordSource] incremental shared frame admission is atomic before prefix adoption") {
	TestRecordSourceAccess::incremental_shared_frame();
}
TEST_CASE("[RecordSource] original worker batches seal reserve settle and count fresh serials") {
	TestRecordSourceAccess::incremental_worker_batch();
}

TEST_CASE("[RecordSource] deferred roots survive busy void-free retries until original owner removal") {
	TestRecordSourceAccess::deferred_free_busy_root();
}
TEST_CASE("[RecordSource] copy source capture survives segment retry without replaying a closed graph") {
	TestRecordSourceAccess::copy_source_segment_retry();
}
TEST_CASE("[RecordSource] enrollment pool refusal restores the original Registry lock and recording pin") {
	TestRecordSourceAccess::enrollment_pool_refusal_restores_registry();
}

TEST_CASE("[RecordSource] hidden native root observes a failed prefix and returns its original resource owner") {
	TestRecordSourceAccess::hidden_root_failure_retirement();
}
TEST_CASE("[RecordSource] successful hidden root never maps a graph missing its original main source") {
	TestRecordSourceAccess::hidden_root_never_maps_missing_main();
}

TEST_CASE("[RecordSource] independent copy admission preserves reserved main slots and cleanup") {
	TestRecordSourceAccess::copy_credits_preserve_main_cleanup();
}
