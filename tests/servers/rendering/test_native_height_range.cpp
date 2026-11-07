/**************************************************************************/
/*  test_native_height_range.cpp                                          */
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

// These controls exercise the original RD reducers, not actual GPU completion.
#include "core/os/os.h"
#include "servers/rendering/native_height_range.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/renderer_rd/height_capture_checked_sink.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

#include <cstdio>
#include <cstdlib>

TEST_FORCE_LINK(test_native_height_range)

class TestNativeHeightRangeAccess {
	using RDD = RenderingDeviceDriver;
	static void child_require(bool condition) {
		if (!condition) {
			std::fprintf(stderr, "NHR_FINALIZE_ASSERTION_FAILED\n");
			std::fflush(stderr);
			std::_Exit(89);
		}
	}
	class Driver : public TransferTestDriverBase {
	public:
		uint32_t retains = 0, polls = 0, releases = 0, recycles = 0, issues = 0, command_ends = 0, segment_ends = 0;
		bool retain_busy = false, release_busy = false, release_fail = false;
		bool finalize_sentinel = false;
		RDD::FencePollResult completion;
		bool native_call_observation_supported() const override { return true; }
		bool native_issue_bound(NativeIssueKind kind, uint32_t commands, uint32_t &bound) const override {
			if (kind != NativeIssueKind::MAIN_SLOT || commands != 1) {
				return false;
			}
			bound = 1;
			return true;
		}
		Error native_call_retain(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++retains;
			return retain_busy ? ERR_BUSY : OK;
		}
		FencePollResult native_call_poll(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++polls;
			return completion;
		}
		FencePollResult native_call_wait(uint64_t serial) override { return native_call_poll(serial); }
		Error native_call_recycle(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++recycles;
			return OK;
		}
		Error native_call_recycle_wait(uint64_t serial) override { return native_call_recycle(serial); }
		void command_pool_free(CommandPoolID) override {
			if (finalize_sentinel) {
				std::fprintf(stderr, "NHR_DEPENDENT_FREE\n");
				std::fflush(stderr);
				std::_Exit(87);
			}
			CRASH_NOW_MSG("Unexpected native-height fixture pool free.");
			std::abort();
		}
		void command_buffer_end(CommandBufferID) override { ++command_ends; }
		void end_segment() override { ++segment_ends; }
		Error command_queue_execute_and_present(CommandQueueID, VectorView<SemaphoreID>, VectorView<CommandBufferID>, VectorView<SemaphoreID>, FenceID, VectorView<SwapChainID>) override {
			++issues;
			record_queue_call_report(TestNativeHeightRangeAccess::report());
			return OK;
		}
		Error native_call_release(uint64_t serial) override {
			if (finalize_sentinel) {
				child_require(serial == 71);
			} else {
				CHECK(serial == 71);
			}
			++releases;
			if (release_fail) {
				return FAILED;
			}
			return release_busy ? ERR_BUSY : OK;
		}
	};
	struct Fixture {
		Driver driver;
		RenderingDevice rd;
		Fixture() {
			rd.driver = &driver;
			rd.is_main_instance = true;
			rd.render_thread_id = Thread::get_caller_id();
			rd.frames.resize(1);
			rd.frames[0].qualification_epoch = 1;
			rd.frames[0].primary_command_buffer_begun = true;
		}
		~Fixture() {
			// Test-owned synthetic rows only. Actual resource/graph cases must use
			// original explicit return and ordinary disposal before detaching.
			rd.native_height_ranges.clear();
			rd.frames.clear();
			rd.driver = nullptr;
		}
	};
	static Ref<NativeHeightRange> sealed(Fixture &f) {
		Ref<NativeHeightRange> range;
		REQUIRE(f.rd._native_height_range_prepare(range) == OK);
		REQUIRE(range.is_valid());
		// Deliberately synthetic sealed member: only reducer tests use this.
		// Actual graph mapping acceptance is a separate normal/bounded test.
		range->graph_count = 1;
		auto &member = range->graphs[0];
		member.frame = 0;
		member.frame_epoch = f.rd.frames[0].qualification_epoch;
		member.state = NativeHeightRange::MemberState::Sealed;
		member.slot = 2;
		member.command_buffer = RDD::CommandBufferID(45);
		range->recording_closed = true;
		range->has_recorded_effect = true;
		f.rd.frames[0].native_height_ranges.push_back(range);
		return range;
	}
	static RDD::QueueCallReport report() {
		RDD::QueueCallReport report;
		RDD::NativeCallFact fact;
		fact.serial = 71;
		fact.completion_serial = 901;
		fact.completion = RDD::CompletionCapability::CommandBuffer;
		fact.issue = RDD::IssueFact::KnownAccepted;
		report.calls.push_back(fact);
		report.native_issue_count = 1;
		report.issued = true;
		return report;
	}
	static Error issue(Fixture &f, uint32_t slot, uint64_t buffer, Error error, const RDD::QueueCallReport &report) {
		return f.rd._native_height_issue_slot(0, f.rd.frames[0].qualification_epoch, slot, RDD::CommandBufferID(buffer), error, report);
	}
	static Error observe(Fixture &f, const Ref<NativeHeightRange> &range, NativeHeightRange::Snapshot &out) {
		RenderingDevice::NativeDisposalBudget budget{ 64, 64, UINT64_MAX };
		RenderingDevice::NativeDisposalUsage usage;
		return f.rd._native_height_range_observe(range, budget, usage, out);
	}
	static Error give_back(Fixture &f, const Ref<NativeHeightRange> &range) {
		RenderingDevice::NativeDisposalBudget budget{ 64, 64, UINT64_MAX };
		RenderingDevice::NativeDisposalUsage usage;
		return f.rd._native_height_range_return(range, budget, usage);
	}

public:
	static void checked_sink_failure_terminal_controls() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		using Effect = RenderingDevice::NativeAppendEffect;
		{
			Fixture f;
			Sink sink(f.rd, 2, 0);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			CHECK(f.rd._native_height_range_close_checked_capture(range, sink) == ERR_INVALID_PARAMETER);
			CHECK_FALSE(range->recording_closed);
			// The original checked Begin refused before effect. Its error survives
			// explicit unwind, but an empty physical prefix has nothing to poll.
			sink.business_count = 1;
			sink.begin_attempted = true;
			sink.business[0].operation = Sink::Operation::Begin;
			sink.business[0].begin.error = ERR_CANT_CREATE;
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) == ERR_CANT_CREATE);
			sink.reject(ERR_CANT_CREATE);
			CHECK(sink.unwind() == ERR_CANT_CREATE);
			CHECK(range->recording_closed);
			CHECK_FALSE(range->has_recorded_effect);
			CHECK_FALSE(range->uncaptured_prefix);
			CHECK(give_back(f, range) == OK);
			CHECK_FALSE(range->native_complete);
			CHECK(f.driver.polls == 0);
		}
		{
			Fixture f;
			Sink sink(f.rd, 2, 0);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			sink.business_count = 1;
			sink.business[0].operation = Sink::Operation::Begin;
			sink.business[0].begin.error = ERR_CANT_CREATE;
			REQUIRE(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) == ERR_CANT_CREATE);
			sink.reject(ERR_CANT_CREATE);
			sink.cleanup_attempted = true;
			// Sticky business error does not refuse the independent cleanup call.
			CHECK(sink.enter_checked_call() == OK);
			sink.cleanup.draw.effect = Effect::Unknown;
			sink.cleanup.draw.error = FAILED;
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, true) == ERR_CANT_CREATE);
			sink.restoration_unknown = true;
			CHECK(sink.unwind() == ERR_CANT_CREATE);
			CHECK(range->recording_closed);
			CHECK(range->has_recorded_effect);
			CHECK(range->uncaptured_prefix);
			CHECK(range->append_error == ERR_CANT_CREATE);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK_FALSE(range->prefix_settled);
			CHECK(f.driver.releases == 0);
		}
		{
			// A physically settled original source does not make an unknown
			// cleanup prefix returnable. Keep the source positive so this tests
			// the terminal distinction rather than the empty-range fast path.
			Fixture f;
			auto range = sealed(f);
			range->recording_closed = false;
			CHECK(issue(f, 2, 45, OK, report()) == OK);
			CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
			f.driver.completion.completed = true;
			f.driver.completion.completed_serial = 901;
			NativeHeightRange::Snapshot settled;
			CHECK(observe(f, range, settled) == OK);
			CHECK(settled.native_complete);
			Sink sink(f.rd, 2, 0);
			sink.native_range = range;
			range->capture_sink = &sink;
			range->capture_frame = 0;
			range->capture_epoch = f.rd.frames[0].qualification_epoch;
			sink.terminal_requested = true;
			sink.cleanup_attempted = true;
			sink.restoration_unknown = true;
			sink.cleanup.draw.effect = Effect::Unknown;
			sink.cleanup.draw.error = FAILED;
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, true) == OK);
			CHECK(f.rd._native_height_range_close_checked_capture(range, sink) == ERR_INVALID_DATA);
			CHECK(range->uncaptured_prefix);
			range->native_complete = false;
			range->prefix_settled = false;
			NativeHeightRange::Snapshot out;
			CHECK(observe(f, range, out) == ERR_INVALID_DATA);
			CHECK_FALSE(out.native_complete);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK_FALSE(range->observers_returned);
			CHECK(f.driver.releases == 0);
		}
	}

	static void checked_sink_provenance_and_missing_members() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		using Effect = RenderingDevice::NativeAppendEffect;
		for (int fault = 0; fault < 4; ++fault) {
			CAPTURE(fault);
			Fixture f;
			Sink sink(f.rd, 2, 1);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			sink.business_count = 1;
			sink.business[0].operation = Sink::Operation::UniformSet;
			sink.business[0].bind.capture_complete = true;
			sink.business[0].bind.state_accepted = true;
			if (fault == 0) {
				// A copied valid-looking handle from a different graph is not a
				// member. contains_record must reject it before source mapping.
				sink.business[0].bind.effect = Effect::Recorded;
				sink.business[0].bind.bind_handle = { 99, 1, 3, 0, 0 };
			} else if (fault == 1) {
				++f.rd.frames[0].qualification_epoch;
			} else if (fault == 2) {
				REQUIRE(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) == OK);
				CHECK(range->graph_count == 0); // Deferred bind adds no graph row.
			} else {
				sink.business[0].bind.transfer_count = 1;
				sink.business[0].transfer_count = 1;
				sink.transfer_count = 1;
			}
			CHECK(f.rd._native_height_range_enroll_original_record(range, sink, 0, false) != OK);
			sink.reject(range->append_error);
			sink.unwind();
			CHECK(range->uncaptured_prefix);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK_FALSE(range->native_complete);
			CHECK(f.driver.polls == 0);
		}
	}

	[[noreturn]] static void actual_finalize_child(const String &mode) {
		Driver driver;
		driver.finalize_sentinel = true;
		RenderingDevice *const previous_singleton = RenderingDevice::get_singleton();
		auto *rd = memnew(RenderingDevice);
		rd->driver = &driver;
		rd->is_main_instance = true;
		rd->render_thread_id = Thread::get_caller_id();
		rd->frames.resize(1);
		auto &frame = rd->frames[0];
		frame.qualification_epoch = 1;
		frame.primary_command_buffer_begun = true;
		Ref<NativeHeightRange> range;
		const auto require = child_require;
		require(RenderingDevice::get_singleton() == (previous_singleton ? previous_singleton : rd));
		require(rd->_native_height_range_prepare(range) == OK);
		// Exact synthetic sealed row for CPU lifecycle control only. The independent
		// Metal fixture already proves real graph/issue/callback membership.
		range->graph_count = 1;
		auto &member = range->graphs[0];
		member.frame = 0;
		member.frame_epoch = frame.qualification_epoch;
		member.state = NativeHeightRange::MemberState::Sealed;
		member.slot = 0;
		member.command_buffer = RDD::CommandBufferID(45);
		range->recording_closed = true;
		range->has_recorded_effect = true;
		frame.native_height_ranges.push_back(range);
		const auto original = report();
		require(rd->_native_height_issue_slot(0, 1, 0, member.command_buffer, OK, original) == OK);
		frame.chain_reports.push_back(original);
		// This frame owns an issued no-fence call, no open/unsent command buffer.
		// Do not set finalization_started/drained or cached native_complete here.
		frame.primary_command_buffer_begun = false;
		driver.completion.completed = true;
		driver.completion.completed_serial = 901;
		driver.release_busy = true;
		std::fprintf(stderr, "NHR_FINALIZE_BEFORE\n");
		std::fflush(stderr);
		rd->finalize(); // ACTUAL public entry; reaches original frame/worker drain.
		require(rd->native_height_finalization_started);
		require(rd->native_height_finalization_drained);
		require(!rd->native_finalization_returned && rd->driver == &driver);
		require(rd->native_height_ranges.size() == 1 && range->native_complete);
		require(!range->observers_returned && range->return_cursor == 0);
		require(driver.releases == 1 && driver.recycles == 1);
		const auto first_polls = driver.polls;
		const auto first_issues = driver.issues;
		const auto first_command_ends = driver.command_ends;
		const auto first_segment_ends = driver.segment_ends;
		rd->finalize(); // ACTUAL repeat; still blocked before graph/resource teardown.
		require(!rd->native_finalization_returned && rd->driver == &driver);
		require(rd->native_height_ranges.size() == 1 && !range->observers_returned);
		require(range->return_cursor == 0 && driver.releases == 2);
		require(driver.polls == first_polls && driver.issues == first_issues);
		require(driver.command_ends == first_command_ends && driver.segment_ends == first_segment_ends);
		require(driver.recycles == 1);
		std::fprintf(stderr, "NHR_FINALIZE_BUSY_RETRY releases=2 recycle=1 owner_live=1 returned=0\n");
		std::fflush(stderr);
		if (mode == "last_ref") {
			std::fprintf(stderr, "NHR_BEFORE_LAST_REF\n");
			std::fflush(stderr);
			memdelete(rd); // ACTUAL destructor: finalize BUSY, then LAST_REF _Exit78.
			std::_Exit(88); // A returned destructor is an invalid positive result.
		}
		std::_Exit(0); // Isolated blocked owner: deliberately no unsafe detach/free.
	}

	static void wrong_slot_buffer_and_empty_busy() {
		Fixture f;
		auto range = sealed(f);
		// Deleting exact slot/buffer matching makes these controls fail.
		CHECK(issue(f, 1, 45, OK, report()) == OK);
		CHECK(issue(f, 2, 46, OK, report()) == OK);
		CHECK(range->source_count == 0);
		CHECK_FALSE(range->graphs[0].issue_attempted);
		CHECK(issue(f, 2, 45, ERR_BUSY, {}) == ERR_BUSY);
		CHECK(range->source_count == 0);
		CHECK_FALSE(range->graphs[0].issue_attempted);
		CHECK(f.driver.retains == 0);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(range->source_count == 1);
		CHECK(range->graphs[0].issue_attempted);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		CHECK(f.driver.retains == 1);
	}
	static void callback_identity_and_unknown_refinement() {
		Fixture f;
		auto range = sealed(f);
		auto actual = report();
		actual.calls.write[0].issue = RDD::IssueFact::IssuedUnknown;
		CHECK(issue(f, 2, 45, OK, actual) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		NativeHeightRange::Snapshot out;
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 902;
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(observe(f, range, out) == ERR_BUSY);
		CHECK_FALSE(out.native_complete);
		CHECK(give_back(f, range) == ERR_BUSY);
		CHECK(f.driver.releases == 0);
		f.driver.completion.completed_serial = 901;
		f.driver.completion.resolved_issue = RDD::IssueFact::NotIssued;
		CHECK(observe(f, range, out) == ERR_BUSY);
		CHECK_FALSE(out.native_complete);
		f.driver.completion.resolved_issue = RDD::IssueFact::KnownAccepted;
		CHECK(observe(f, range, out) == OK);
		CHECK(out.native_complete);
		f.driver.release_busy = true;
		CHECK(give_back(f, range) == ERR_BUSY);
		CHECK(range->return_cursor == 0);
		f.driver.release_busy = false;
		CHECK(give_back(f, range) == OK);
		const uint32_t polls = f.driver.polls;
		const uint32_t releases = f.driver.releases;
		CHECK(give_back(f, range) == OK);
		CHECK(observe(f, range, out) == OK);
		CHECK(out.native_complete);
		CHECK(out.observers_returned);
		CHECK(f.driver.polls == polls);
		CHECK(f.driver.releases == releases);
	}
	static void retain_busy_and_two_independent_cells() {
		Fixture f;
		auto first = sealed(f);
		auto second = sealed(f);
		f.driver.retain_busy = true;
		CHECK(issue(f, 2, 45, OK, report()) == ERR_BUSY);
		CHECK(first->source_count == 1);
		CHECK(second->source_count == 1);
		CHECK(f.rd._native_height_retain_frame_sources(0) == ERR_BUSY);
		CHECK_FALSE(first->sources[0].retained);
		CHECK_FALSE(second->sources[0].retained);
		f.driver.retain_busy = false;
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		CHECK(first->sources[0].retained);
		CHECK(second->sources[0].retained);
		const uint32_t retains = f.driver.retains;
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		CHECK(f.driver.retains == retains);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, first, out) == OK);
		CHECK(give_back(f, first) == OK);
		CHECK(first->sources[0].observer_returned);
		CHECK_FALSE(second->sources[0].observer_returned);
		CHECK(observe(f, second, out) == OK);
		CHECK(out.native_complete);
		CHECK(give_back(f, second) == OK);
		CHECK(f.driver.releases == 2);
	}
	static void old_epoch_and_empty_close() {
		Fixture f;
		auto range = sealed(f);
		++f.rd.frames[0].qualification_epoch;
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(range->source_count == 0);
		CHECK_FALSE(range->graphs[0].issue_attempted);
		Ref<NativeHeightRange> empty;
		CHECK(f.rd._native_height_range_prepare(empty) == OK);
		CHECK(f.rd._native_height_range_close(empty) == OK);
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, empty, out) == OK);
		CHECK(out.recording_closed);
		CHECK_FALSE(out.has_recorded_effect);
		CHECK_FALSE(out.native_complete);
		CHECK(give_back(f, empty) == OK);
		CHECK(f.driver.retains == 0);
		CHECK(f.driver.releases == 0);
	}
	static void actual_issue_hooks_and_retain_barriers() {
		for (bool bounded : { false, true }) {
			Fixture f;
			auto range = sealed(f);
			range->graphs[0].slot = 0;
			f.rd.frames[0].command_buffer = RDD::CommandBufferID(45);
			f.driver.retain_busy = true;
			if (bounded) {
				// Start at the original already-sealed continuation's actual issue
				// phase; no test enrollment hook is installed in production code.
				f.rd.native_disposal_active = true;
				auto &cursor = f.rd.native_disposal_continuation;
				cursor.serial = 1;
				cursor.phase = 2;
				cursor.slot_count = 1;
				RenderingDevice::NativeDisposalBudget budget{ 64, 64, UINT64_MAX };
				RenderingDevice::NativeDisposalUsage usage;
				CHECK(f.rd._native_disposal_maintenance_step(budget, usage) == ERR_BUSY);
				CHECK(cursor.phase == 3);
				CHECK(cursor.slot == 1);
			} else {
				CHECK(f.rd.execute_chained_cmds(false, {}, {}));
			}
			CHECK(f.driver.issues == 1);
			CHECK(range->source_count == 1);
			CHECK(range->graphs[0].issue_attempted);
			CHECK_FALSE(range->sources[0].retained);
			CHECK((bounded ? f.rd._native_disposal_stall_step(0) : f.rd._stall_for_frame(0, true)) == ERR_BUSY);
			CHECK(f.driver.recycles == 0);
			CHECK(f.driver.polls == 0);
			f.driver.retain_busy = false;
			CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
			CHECK(range->sources[0].retained);
			CHECK(f.driver.issues == 1);
			f.driver.completion.completed = true;
			f.driver.completion.completed_serial = 901;
			NativeHeightRange::Snapshot out;
			CHECK(observe(f, range, out) == OK);
			CHECK(out.native_complete);
			CHECK(give_back(f, range) == OK);
		}
	}
	static void sealed_flush_retry_blocks_getter_append() {
		Fixture f;
		auto range = sealed(f);
		range->graphs[0].slot = 0;
		auto &frame = f.rd.frames[0];
		frame.command_buffer = RDD::CommandBufferID(45);
		frame.qualification_graph_closed = frame.qualification_segment_closed = true;
		f.driver.retain_busy = true;
		CHECK(f.rd._execute_frame(false) == OK);
		CHECK(f.driver.issues == 1);
		CHECK(frame.fence_signaled);
		CHECK(f.rd._flush_and_stall_for_all_frames(false) == ERR_BUSY);
		CHECK(f.rd._recording_error() == ERR_BUSY);
		// This public synchronous getter refuses at its original recording guard
		// before creating a temporary buffer or appending another readback.
		ERR_PRINT_OFF;
		CHECK(f.rd.buffer_get_data(RID()).is_empty());
		ERR_PRINT_ON;
		CHECK(f.rd._end_frame() == OK);
		CHECK(f.rd._execute_frame(false) == OK);
		CHECK(f.driver.issues == 1);
		CHECK(f.driver.command_ends == 0);
		CHECK(f.driver.segment_ends == 0);
		CHECK(f.driver.recycles == 0);
	}
	static void finite_capacity_and_foreign_range_refusal() {
		Fixture f;
		LocalVector<Ref<NativeHeightRange>> prepared;
		prepared.resize(RenderingDevice::NATIVE_HEIGHT_RANGE_CAPACITY);
		for (uint32_t i = 0; i < RenderingDevice::NATIVE_HEIGHT_RANGE_CAPACITY; ++i) {
			CHECK(f.rd._native_height_range_prepare(prepared[i]) == OK);
			CHECK(prepared[i].is_valid());
		}
		Ref<NativeHeightRange> exhausted;
		CHECK(f.rd._native_height_range_prepare(exhausted) == ERR_BUSY);
		CHECK(exhausted.is_null());
		Fixture foreign_fixture;
		Ref<NativeHeightRange> foreign;
		REQUIRE(foreign_fixture.rd._native_height_range_prepare(foreign) == OK);
		NativeHeightRange::Snapshot out;
		RenderingDevice::NativeDisposalUsage foreign_usage;
		CHECK(f.rd._native_height_range_observe(foreign, RenderingDevice::NativeDisposalBudget{ 64, 64, UINT64_MAX }, foreign_usage, out) == ERR_INVALID_PARAMETER);
		CHECK_FALSE(out.native_complete);
	}
	static void post_drain_finalization_busy_cursor() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, range, out) == OK);
		CHECK(out.native_complete);
		f.rd.native_height_finalization_started = true;
		f.rd.native_height_finalization_drained = true;
		f.driver.release_busy = true;
		CHECK(f.rd._native_height_ranges_prepare_finalization() == ERR_BUSY);
		CHECK(f.rd.native_height_ranges.size() == 1);
		CHECK_FALSE(range->observers_returned);
		const uint32_t issues = f.driver.issues;
		const uint32_t polls = f.driver.polls;
		f.driver.release_busy = false;
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.rd.native_height_ranges.is_empty());
		CHECK(range->observers_returned);
		CHECK(f.driver.issues == issues);
		CHECK(f.driver.polls == polls);
		CHECK(f.driver.releases == 2);
		// A retry of this post-drain phase never re-enters graph end or issue.
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.driver.issues == issues);
		CHECK(f.driver.polls == polls);
	}
	static void finalization_does_not_close_live_checked_sink() {
		using Sink = RendererRD::HeightCaptureCheckedSink;
		for (bool pending_result : { false, true }) {
			CAPTURE(pending_result);
			Fixture f;
			Sink sink(f.rd, 2, 0);
			Ref<NativeHeightRange> range;
			REQUIRE(sink.prepare_native_range(range) == OK);
			if (pending_result) {
				// Model a checked call which has returned an unknown prefix but
				// has not enrolled yet. Its empty range arrays are not no-effect.
				sink.business_count = 1;
				sink.business[0].begin.effect = RenderingDevice::NativeAppendEffect::Unknown;
				sink.restoration_unknown = true;
				sink.reject(ERR_CANT_CREATE);
			}
			for (int retry = 0; retry < 2; ++retry) {
				CHECK(f.rd._native_height_ranges_prepare_finalization() == ERR_BUSY);
				CHECK_FALSE(range->recording_closed);
				CHECK_FALSE(range->observers_returned);
				CHECK(f.rd.native_height_ranges.size() == 1);
				CHECK(f.rd.frames[0].native_height_ranges.size() == 1);
				CHECK(f.driver.polls == 0);
				CHECK(f.driver.releases == 0);
			}
			sink.unwind(); // Only the original terminal path may freeze custody.
			CHECK(range->recording_closed);
			if (pending_result) {
				CHECK(range->uncaptured_prefix);
				CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
				CHECK(f.rd.native_height_ranges.size() == 1);
				CHECK_FALSE(range->observers_returned);
			} else {
				CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
				CHECK(f.rd.native_height_ranges.is_empty());
				CHECK(f.rd.frames[0].native_height_ranges.is_empty());
				CHECK(range->observers_returned);
				CHECK_FALSE(range->native_complete);
			}
		}
	}
	static void preflight_zero_effect_restrictions() {
		Fixture f;
		Ref<NativeHeightRange> range;
		CHECK(f.rd._native_height_range_prepare(range) == OK);
		NativeDisposalWitness depth, framebuffer;
		CHECK(f.rd._native_height_range_record_depth_clear(range, depth, framebuffer, 1.0f) == ERR_INVALID_PARAMETER);
		CHECK_FALSE(range->has_recorded_effect);
		f.rd.draw_list.active = true;
		Ref<NativeHeightRange> blocked;
		CHECK(f.rd._native_height_range_prepare(blocked) == ERR_BUSY);
		f.rd.draw_list.active = false;
		f.rd.frames[0].qualification_graph_closed = true;
		CHECK(f.rd._native_height_range_prepare(blocked) == ERR_BUSY);
		f.rd.frames[0].qualification_graph_closed = false;
		CHECK(f.rd._native_height_range_close(range) == OK);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
	}

	static void known_no_effect_failure_settles_but_keeps_error() {
		Fixture f;
		Ref<NativeHeightRange> range;
		REQUIRE(f.rd._native_height_range_prepare(range) == OK);
		// Model the original checked begin's known pre-effect refusal. No graph
		// handle/source is fabricated and the cell's sticky failure survives.
		range->recording_started = true;
		range->append_error = ERR_CANT_CREATE;
		f.rd.frames[0].native_height_ranges.push_back(range);
		CHECK(f.rd._native_height_range_close(range) == ERR_CANT_CREATE);
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.append_error == ERR_CANT_CREATE);
		CHECK_FALSE(out.has_recorded_effect);
		CHECK_FALSE(out.native_complete);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.rd.native_height_ranges.is_empty());
		CHECK(f.rd.frames[0].native_height_ranges.is_empty());
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.append_error == ERR_CANT_CREATE);
		CHECK_FALSE(out.native_complete);
		CHECK(f.driver.polls == 0);
		CHECK(f.driver.retains == 0);
		CHECK(f.driver.releases == 0);
		Ref<NativeHeightRange> unknown;
		REQUIRE(f.rd._native_height_range_prepare(unknown) == OK);
		unknown->uncaptured_prefix = true;
		unknown->append_error = ERR_INVALID_DATA;
		CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
		CHECK(f.rd.native_height_ranges.size() == 1);
		CHECK_FALSE(unknown->observers_returned);
	}
	static void failed_source_blocks_finalization() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		f.driver.completion.native_error = true;
		f.driver.completion.native_code = -8;
		NativeHeightRange::Snapshot out;
		CHECK(observe(f, range, out) == FAILED);
		CHECK_FALSE(out.native_complete);
		CHECK(out.completion_error == FAILED);
		CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
		CHECK(f.rd.driver == &f.driver);
		CHECK_FALSE(f.rd.native_finalization_returned);
		CHECK(f.rd.native_height_ranges.size() == 1);
		CHECK(f.driver.releases == 0);
		// There is no manufactured "device lost => complete" escape hatch.
		// Destructive-finalize failure is tested in a subprocess, never in this
		// doctest process: original frame wait CRASH_NOW / LAST_REF fail-stop.
	}
	static void settled_prefix_keeps_business_error_but_returns_sources() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		// The checked sink may have captured a real native prefix before a
		// later business operation failed. Keep that sticky error, but allow
		// mechanical observer retirement once every source is settled.
		range->append_error = ERR_CANT_CREATE;
		CHECK(f.rd._native_height_range_close(range) == ERR_CANT_CREATE);
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.prefix_settled);
		CHECK_FALSE(out.native_complete);
		CHECK(out.append_error == ERR_CANT_CREATE);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == OK);
		CHECK(f.rd.native_height_ranges.is_empty());
		CHECK(range->observers_returned);
		CHECK(f.driver.releases == 1);
	}
	static void failed_prefix_release_is_sticky_and_not_replayed() {
		Fixture f;
		auto range = sealed(f);
		CHECK(issue(f, 2, 45, OK, report()) == OK);
		CHECK(f.rd._native_height_retain_frame_sources(0) == OK);
		f.driver.completion.completed = true;
		f.driver.completion.completed_serial = 901;
		NativeHeightRange::Snapshot out;
		range->append_error = ERR_CANT_CREATE;
		CHECK(f.rd._native_height_range_close(range) == ERR_CANT_CREATE);
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		f.driver.release_fail = true;
		CHECK(give_back(f, range) == FAILED);
		CHECK(f.driver.releases == 1);
		f.driver.release_fail = false;
		CHECK(give_back(f, range) == FAILED);
		CHECK(f.driver.releases == 1);
		CHECK(f.rd._native_height_ranges_prepare_finalization() == ERR_CANT_CREATE);
		CHECK(observe(f, range, out) == ERR_CANT_CREATE);
		CHECK(out.return_error == FAILED);
		CHECK(f.driver.releases == 1);
		CHECK_FALSE(range->observers_returned);
		CHECK(f.rd.native_height_ranges.size() == 1);
	}
	static void business_failure_does_not_settle_unsafe_prefix() {
		enum class Fault { Mapping, Issue, Retention, Completion, Uncaptured, Unresolved, WrongSerial };
		for (Fault fault : { Fault::Mapping, Fault::Issue, Fault::Retention, Fault::Completion, Fault::Uncaptured, Fault::Unresolved, Fault::WrongSerial }) {
			CAPTURE(int(fault));
			Fixture f;
			auto range = sealed(f); // Synthetic reducer member; no new issuer.
			auto original = report();
			if (fault == Fault::Unresolved) {
				original.calls.write[0].issue = RDD::IssueFact::IssuedUnknown;
			}
			REQUIRE(issue(f, 2, 45, OK, original) == OK);
			f.driver.completion.completed = true;
			f.driver.completion.completed_serial = fault == Fault::WrongSerial ? 902 : 901;
			f.driver.completion.resolved_issue = fault == Fault::Unresolved ? RDD::IssueFact::IssuedUnknown : RDD::IssueFact::KnownAccepted;
			if (fault == Fault::Mapping) {
				range->mapping_error = ERR_INVALID_DATA;
			} else if (fault == Fault::Issue) {
				range->issue_error = FAILED;
			} else if (fault == Fault::Retention) {
				range->retention_error = FAILED;
			} else if (fault == Fault::Completion) {
				f.driver.completion.native_error = true;
			} else if (fault == Fault::Uncaptured) {
				range->uncaptured_prefix = true;
			}
			range->append_error = ERR_CANT_CREATE;
			NativeHeightRange::Snapshot out;
			CHECK(observe(f, range, out) == ERR_CANT_CREATE);
			CHECK(out.append_error == ERR_CANT_CREATE);
			CHECK_FALSE(out.prefix_settled);
			CHECK_FALSE(out.native_complete);
			CHECK(give_back(f, range) == ERR_BUSY);
			CHECK(f.rd._native_height_ranges_prepare_finalization() != OK);
			CHECK_FALSE(range->observers_returned);
			CHECK(f.rd.native_height_ranges.size() == 1);
			CHECK(f.rd.frames[0].native_height_ranges.size() == 1);
			CHECK(f.driver.releases == 0);
		}
	}
};

TEST_CASE("[RenderingDevice][NativeHeightRange] exact slot buffer and empty BUSY") {
	TestNativeHeightRangeAccess::wrong_slot_buffer_and_empty_busy();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] checked sink failed begin and unknown cleanup remain distinct") {
	TestNativeHeightRangeAccess::checked_sink_failure_terminal_controls();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] checked sink provenance duplicate and unsupported transfer quarantine") {
	TestNativeHeightRangeAccess::checked_sink_provenance_and_missing_members();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] original callback identity and observer return") {
	TestNativeHeightRangeAccess::callback_identity_and_unknown_refinement();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] native failure preserves finalization owner") {
	TestNativeHeightRangeAccess::failed_source_blocks_finalization();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] settled native prefix returns despite sticky business error") {
	TestNativeHeightRangeAccess::settled_prefix_keeps_business_error_but_returns_sources();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] failed observer return is sticky and not replayed") {
	TestNativeHeightRangeAccess::failed_prefix_release_is_sticky_and_not_replayed();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] business failure cannot settle unsafe native prefix") {
	TestNativeHeightRangeAccess::business_failure_does_not_settle_unsafe_prefix();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] retain BUSY and shared source observers") {
	TestNativeHeightRangeAccess::retain_busy_and_two_independent_cells();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] old frame epoch and empty closure") {
	TestNativeHeightRangeAccess::old_epoch_and_empty_close();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] known no-effect failure settles without native success") {
	TestNativeHeightRangeAccess::known_no_effect_failure_settles_but_keeps_error();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] ordinary and bounded actual issue hooks retain before recycle") {
	TestNativeHeightRangeAccess::actual_issue_hooks_and_retain_barriers();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] sealed synchronous getter retry cannot append or issue again") {
	TestNativeHeightRangeAccess::sealed_flush_retry_blocks_getter_append();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] finite capacity and foreign range refusal") {
	TestNativeHeightRangeAccess::finite_capacity_and_foreign_range_refusal();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] preflight refuses invalid witnesses and active recording") {
	TestNativeHeightRangeAccess::preflight_zero_effect_restrictions();
}

TEST_CASE("[RenderingDevice][NativeHeightRange] post-drain finalization BUSY resumes without reissue") {
	TestNativeHeightRangeAccess::post_drain_finalization_busy_cursor();
}
TEST_CASE("[RenderingDevice][NativeHeightRange] finalization cannot guess live checked sink closed or empty") {
	TestNativeHeightRangeAccess::finalization_does_not_close_live_checked_sink();
}

// The normal full suite performs no destructive work. Only the isolated runner
// enables this mode; all process exits below are confined to its child host.
TEST_CASE("[RenderingDevice][NativeHeightRange] actual finalize child") {
	const String mode = OS::get_singleton()->get_environment("GODOT_NHR_FINALIZE_CHILD");
	if (mode == "busy" || mode == "last_ref") {
		TestNativeHeightRangeAccess::actual_finalize_child(mode);
	}
}
